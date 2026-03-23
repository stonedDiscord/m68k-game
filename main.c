/*
 * example.c – HD63484 usage example
 *
 * Configured to match the ADP/Merkur arcade hardware as emulated in MAME
 * (adp.cpp driver: quickjac, skattv, skattva, fashiong etc.)
 *
 * System specs:
 *   CPU  : 68EC000 @ 8 MHz
 *   ACRTC: HD63484, VRAM at 0x00000 in the chip's address space (128KB RAM)
 *   Screen: 384 x 280 total raster, 60 Hz, full visible area
 *   Colors: 4 bits per pixel (16 colours), palette index 0-15
 *
 * Pixel geometry with 4bpp + GAI_INC1:
 *   Each memory cycle fetches 1 word = 4 pixels (one nibble each).
 *   Display width  = (HDW+1) cycles * 4 px/cycle = 96 * 4 = 384 px
 *   Display height = SP1 rasters = 280
 *   Memory width   = 384 px / 4 px per word = 96 words per line
 *
 * Palette (ADP hardware RGB formula, from adp.cpp adp_palette()):
 *   r = 0xB8 * bit(i,0) + 0x47 * bit(i,3)
 *   g = 0xB8 * bit(i,1) + 0x47 * bit(i,3)
 *   b = 0xB8 * bit(i,2) + 0x47 * bit(i,3)
 *   -> 4-bit IRGB palette (bit3=intensity, bits2-0=RGB)
 */

#include "duart.h"
#include "audio.h"
#include "gpu.h"
#include "font.h" /* font8x8[128][8] */
#include "pt3player.h"
#include "rtc.h"

#include "io.h"

/* Music data from music_data.c */
extern unsigned char pt3player_main_track_pt3[];
extern unsigned int pt3player_main_track_pt3_len;

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

char rec_a_buffer = 0;
char rec_b_buffer = 0;

/* =========================================================================
 * ISR – placed at the vector address by the linker script (platform.ld).
 *
 * The ISR reads ISR to find which source fired, then handles each one.
 * Reading STOP_CNTR clears the counter/timer interrupt (ISR[3]).
 * Reading RBRA/RBRB clears the receiver-ready interrupt for that channel.
 * ========================================================================= */
bool swapper = true;
static uint32_t timer_ticks = 0;

void handle_timer(void)
{
	func_play_tick(0);
	audio_update_pt3();
	timer_ticks++;
	if (timer_ticks >= 50)
	{ /* 50 * 10 ms = 500 ms */
		timer_ticks = 0;
		OPR_CLR = 0x60; /* Deassert both OP5 and OP6 (both LEDs off) */
		if (swapper)
		{
			OPR_SET = 0x20; /* Assert OP5 (LED1 on) */
		}
		else
		{
			OPR_SET = 0x40; /* Assert OP6 (LED2 on) */
		}
		swapper = !swapper;
	}
}

void __attribute__((interrupt))
duartInterrupt(void)
{
	volatile char status = ISR; /* Snapshot – reading ISR has no side effect */

	/* ---- Channel A receiver ready ---- */
	if (status & ISR_RxRDYA)
	{
		volatile char c = RBRA; /* Reading the buffer clears RxRDYA in ISR  */
		rec_a_buffer = c;		/* Replace with your receive handler         */
	}

	/* ---- Channel B receiver ready ---- */
	if (status & ISR_RxRDYB)
	{
		volatile char c = RBRB;
		rec_b_buffer = c;
	}

	/* ---- Channel A transmitter ready ---- */
	if (status & ISR_TxRDYA)
	{
		/*
		 * The transmit-ready interrupt stays active as long as TBRA is empty.
		 * Disable the interrupt here, or load the next character into TBRA.
		 * Loading a character clears TxRDYA until the shift register accepts
		 * it and the holding register becomes empty again.
		 */
	}

	/* ---- Counter / timer ready ---- */
	if (status & ISR_CTRDY)
	{
		/*
		 * In timer mode ISR[3] is set every second countdown cycle.
		 * Reading STOP_CNTR clears ISR[3] but does NOT stop the timer.
		 * In counter mode, reading STOP_CNTR clears ISR[3] AND stops
		 * the counter.
		 */
		volatile char dummy = STOP_CNTR; /* Clear the C/T interrupt */
		(void)dummy;

		/* Put your periodic tick handler here */
		handle_timer();
	}

	/* ---- Input port change ---- */
	if (status & ISR_IPCNG)
	{
		volatile char ipcr = IPCR; /* Reading IPCR clears ISR[7] */
		(void)ipcr;
	}
}

void __attribute__((interrupt))
vector64(void)
{
	;
}

static inline void enable_interrupts(void)
{
	__asm__ volatile("move.w #0x2000, %%sr" : : : "memory");
}

void display_all_inputs()
{
	scan_inputs();

	for (int n = 0; n < 8; n++)
	{
		uint16_t input_state = read_input(n);
		// Draw label "Input n: 0xXXXX"
		char label[20];
		sprintf(label, "Input %d: 0x%04X", n, input_state);
		hd63484_draw_string(8, 180 + n * 10, label, PAL_WHITE, PAL_BLACK);
	}
	char hex_str[5];
	hd63484_draw_string(150, 180, "YM2149A", PAL_WHITE, PAL_BLACK);
	sprintf(hex_str, "%04X", read_ioa());
	hd63484_draw_string(220, 180, hex_str, PAL_WHITE, PAL_BLACK);
	hd63484_draw_string(150, 190, "YM2149B", PAL_WHITE, PAL_BLACK);
	sprintf(hex_str, "%04X", read_iob());
	hd63484_draw_string(220, 190, hex_str, PAL_WHITE, PAL_BLACK);
}

void dump_input(uint8_t n) {
	uint16_t input_state = read_input(n);
	printf("Input %d: 0x%04X", n, input_state);
}

int main(void)
{
	hd63484_config_t cfg;

	cfg.gbm = GBM_4BPP;
	cfg.ccr_ie = 0;

	cfg.acm = ACM_SINGLE;
	cfg.gai = GAI_INC1; /* 1 word/cycle = 4 pixels at 4bpp */
	cfg.rsm = RSM_NONINTERLACE;
	cfg.ram_mode = 1; /* static RAM, no DRAM refresh      */
	cfg.acp = 0;	  /* display priority                 */

	cfg.dcr = DCR_DSP | DCR_SE1; /* Base screen on, standard DISP */

	cfg.hc = HTOTAL;
	cfg.hsw = HSYNC_W;
	cfg.hds = HDISP_S;
	cfg.hdw = HDISP_W;

	cfg.vc = VTOTAL;
	cfg.vsw = VSYNC_W;
	cfg.vds = VDISP_S;
	cfg.sp1 = SCREEN_RASTERS;

	cfg.mwr1 = MEM_WIDTH;
	cfg.sar1 = VRAM_BASE;
	cfg.draw_base = VRAM_BASE;

	hd63484_init(&cfg);

	ramdac_reset();

	hd63484_set_font(font8x8);

	/* 1. Clear to black */
	hd63484_clear_screen(PAL_GREEN, SCREEN_W, SCREEN_H);

	/* 2. White border */
	hd63484_set_color1(PAL_WHITE);
	hd63484_amove(0, 0);
	hd63484_arct(SCREEN_W - 1, SCREEN_H - 1, AREA_NONE, COL_REG_IND, OPM_REPLACE);

	/* 4. Cyan diagonal line */
	hd63484_draw_line(0, 0, SCREEN_W - 1, SCREEN_H - 1, PAL_CYAN);
	hd63484_draw_line(0, SCREEN_H - 1, SCREEN_W - 1, 0, PAL_MAGENTA);

	/* Text on a coloured background */
	println("Testprogramm fur STELLA Gerate.");
	println("Und andere Banditen");
	println("Test1");
	println("Test2");

	setup_duart();
	enable_interrupts();

	printf("Hello, world!\n");

	/* Initialize PT3 player */
	func_setup_music(pt3player_main_track_pt3, pt3player_main_track_pt3_len, 0, 0);

	uint16_t counter = 0;

	do
	{
		char lnr[15];
		sprintf(lnr, "Popcorn %d. ", counter);
		print_string(lnr);


		switch (rec_a_buffer)
		{
		case 'H':
		case 'h':
			printf("Hallo!\n");
			break;

		case 'T':
		case 't':
		{
			/* Use rtc_get_timespec from rtc.c */
			struct timespec ts = rtc_get_timespec();
			struct tm *tm = localtime(&ts.tv_sec);
			printf("Datum: %02d.%02d.%04d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
			printf("Uhrzeit: %02d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		}

		default:
			if (rec_a_buffer >= '0' && rec_a_buffer <= '7')
			{
				dump_input(rec_a_buffer - '0');
			}
			else if (rec_a_buffer != 0)
			{
				print_string(rec_a_buffer);
			}
			break;
		}

		rec_a_buffer = 0x00;

		counter++;
		if (counter % 10 == 0)
		{
			display_all_inputs();
		}
		if (counter >= 30000)
		{
			// Change the text color after 30000 iterations
			hd63484_draw_string(8, 8, "Das waren 30000 Umdrehungen.", PAL_YELLOW, PAL_BLUE);
			counter = 0;
		}
	} while (counter < 60000);
	return 0;
}
