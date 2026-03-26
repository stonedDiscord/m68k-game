/*
 * main.c – Profitech 3000 Homebrew
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
 */

#include "duart.h"
#include "audio.h"
#include "gpu.h"
#include "pt3player.h"
#include "rtc.h"
#include "tk.h"

#include "io.h"

/* Music data from music_data.c */
extern unsigned char pt3player_main_track_pt3[];
extern unsigned int pt3player_main_track_pt3_len;

#include <stdio.h>
#include <stdbool.h>

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

	hd63484_set_color_bg(PAL_BLACK);
	hd63484_set_color_fg(PAL_WHITE);

	for (int n = 0; n < 8; n++)
	{
		uint16_t input_state = read_input(n);
		// Draw label "Input n: 0xXXXX"
		char label[20];
		sprintf(label, "Input %d: 0x%04X", n, input_state);
		hd63484_draw_string(8, 180 + n * 10, label);
	}
	char hex_str[5];

	hd63484_draw_string(150, 180, "YM2149A");
	sprintf(hex_str, "%04X", read_ioa());
	hd63484_draw_string(220, 180, hex_str);
	hd63484_draw_string(150, 190, "YM2149B");
	sprintf(hex_str, "%04X", read_iob());
	hd63484_draw_string(220, 190, hex_str);
}

void dump_input(uint8_t n)
{
	uint16_t input_state = read_input(n);
	printf("Input %d: 0x%04X", n, input_state);
}

int main(void)
{
	hd63484_init();

	ramdac_reset();

	/* 1. Clear to black */
	hd63484_clear_screen(PAL_BLACK, SCREEN_W, SCREEN_H);

	/* 2. White border */
	hd63484_set_color_bg(PAL_BLACK);
	hd63484_set_color_fg(PAL_WHITE);
	hd63484_amove(0, 0);
	hd63484_arct(SCREEN_W - 1, SCREEN_H - 1, AREA_NONE, COL_REG_IND, OPM_REPLACE);

	/* 4. Cyan diagonal line */
	hd63484_draw_line(0, 0, SCREEN_W - 1, SCREEN_H - 1, PAL_CYAN);
	hd63484_draw_line(0, SCREEN_H - 1, SCREEN_W - 1, 0, PAL_MAGENTA);

	setup_duart();

	/* Text on a coloured background */
	hd63484_set_color_bg(PAL_BLACK);
	hd63484_set_color_fg(PAL_WHITE);
	println("Testprogramm:");
	printf("Testprogramm\n\n");
	println(" ");

	/* Test 1: RAMDAC */
	print_string("Test 1 RAMDAC: ");
	printf("Test 1 RAMDAC: ");
	if (*ramdac_mask == 0xFF)
	{
		hd63484_enable_screen(SCREEN_BASE, VRAM_UPPER, MEM_WIDTH & 0x03FFu);
		{
			uint32_t org_addr = VRAM_UPPER +
								(uint32_t)(SCREEN_RASTERS - 1) * (uint32_t)MEM_WIDTH;
			hd63484_set_origin(0x1 /* DN_BASE */, org_addr, 0);
		}

		hd63484_set_dp(0x1, VRAM_UPPER, 0);
		hd63484_set_rwp(0x1, VRAM_UPPER);
		hd63484_set_color_bg(PAL_GREEN);
		println("OK");
		printf("OK\n");
	}
	else
	{
		hd63484_set_color_bg(PAL_RED);
		println("Nein");
		printf("Nein\n");
	}
	hd63484_set_color_bg(PAL_BLACK);


static const struct tm default_time = {
    .tm_sec  = 30,    // seconds [0, 60]
    .tm_min  = 46,    // minutes [0, 59]
    .tm_hour = 8,   // hours [0, 23]
    .tm_mday = 11,    // day of month [1, 31]
    .tm_mon  = 8,    // months since January [0, 11]
    .tm_year = 101,  // years since 1900 (2024 -> 124)
    .tm_wday = 2,    // day of week [0, 6] (Sunday = 0)
    .tm_yday = 254,    // day of year [0, 365]
    .tm_isdst = 0   // daylight saving time unknown
};

	/* Test 2: RTC */
	print_string("Test 2 RTC: ");
	printf("Test 2 RTC: ");
	struct tm time_rtc;
	if (rtc_get(&time_rtc)!= 0)
	{
		hd63484_set_color_bg(PAL_RED);
		print_string("Falsch\nSetze RTC: ");
		printf("Falsch\nSetze RTC: ");
		rtc_set(&default_time);
	}
	if (rtc_get(&time_rtc) == 0)
	{
		hd63484_set_color_bg(PAL_GREEN);
		print_string("OK\n");
		printf("OK\n");
	}
	else
	{
		hd63484_set_color_bg(PAL_RED);
		print_string("Falsch\n");
		printf("Falsch\n");
	}
	{
	char datum[32];
	sprintf(datum, "Datum %04d-%02d-%02d\nZeit %02d:%02d:%02d\n",
			time_rtc.tm_year + 1900,
			time_rtc.tm_mon + 1,
			time_rtc.tm_mday,
			time_rtc.tm_hour,
			time_rtc.tm_min,
			time_rtc.tm_sec);
	print_string(datum);
	printf(datum);
	}
	hd63484_set_color_bg(PAL_BLACK);

	/* Test 3: TK (Time Keeper) */
	print_string("Test 3 TK: ");
	printf("Test 3 TK: ");
	struct tm time;
	if (tk_get(&time) != 0)
	{
		hd63484_set_color_bg(PAL_RED);
		print_string("Falsch\nSetze TK: ");
		printf("Falsch\nSetze TK: ");
		tk_set(&default_time);
	}
	if (tk_get(&time) == 0)
	{
		hd63484_set_color_bg(PAL_GREEN);
		print_string("OK\n");
		printf("OK\n");
	}
	else
	{
		hd63484_set_color_bg(PAL_RED);
		print_string("Falsch\n");
		printf("Falsch\n");
	}
	{
	char datum[32];
	sprintf(datum, "Datum %04d-%02d-%02d\nZeit %02d:%02d:%02d\n",
			time.tm_year + 1900,
			time.tm_mon + 1,
			time.tm_mday,
			time.tm_hour,
			time.tm_min,
			time.tm_sec);
	print_string(datum);
	printf(datum);
	}
	hd63484_set_color_bg(PAL_BLACK);

	/* Test 4: DUART */
	print_string("Test 4 DUART: ");
	printf("Test 4 DUART (Hallo das bin ich!): ");
	if (SRB & TxRDY)
	{
		hd63484_set_color_bg(PAL_GREEN);
		println("OK");
		printf("OK\n");
	}
	else
	{
		hd63484_set_color_bg(PAL_RED);
		println("Nein");
		printf("Nein\n");
	}
	hd63484_set_color_bg(PAL_BLACK);

	/* Test 5: Video */
	print_string("Test 5 Video (Hallo das bin ich!): ");
	printf("Test 5 Video: ");
	if (hd63484_read_sr() & SR_CED)
	{
		hd63484_set_color_bg(PAL_GREEN);
		println("OK");
		printf("OK\n");
	}
	else
	{
		hd63484_set_color_bg(PAL_RED);
		println("Nein");
		printf("Nein\n");
	}
	hd63484_set_color_bg(PAL_BLACK);

	/* Test 6: RAM */
	print_string("Test 6 SRAM: ");
	printf("Test 6 SRAM: ");
	volatile uint16_t *ramtest_addr = (volatile uint16_t *)0xfc0080;
	*ramtest_addr = 0x1234;
	if (*ramtest_addr == 0x1234)
	{
		hd63484_set_color_bg(PAL_GREEN);
		println("OK");
		printf("OK\n");
	}
	else
	{
		hd63484_set_color_bg(PAL_RED);
		println("Nein");
		printf("Nein\n");
	}
	hd63484_set_color_bg(PAL_BLACK);

	enable_interrupts();

	/* Initialize PT3 player */
	func_setup_music(pt3player_main_track_pt3, pt3player_main_track_pt3_len, 0, 0);

	uint16_t counter = 0;

	/* Animation: springende Kreise in der Mitte des Bildschirms */
	int16_t center_x = SCREEN_W / 2; /* 192 */
	int16_t center_y = SCREEN_H / 2; /* 140 */
	uint8_t anim_frame = 0;

	do
	{
		/* Animation: pulsierender Kreis in der Mitte */
		uint16_t radius = 10 + (anim_frame % 31);

		/* Kreis zeichnen */
		hd63484_set_color_bg(anim_frame % 0x0Fu);
		hd63484_amove(center_x, center_y);
		hd63484_crcl(radius, 1, AREA_NONE, COL_REG_IND, OPM_REPLACE);

		anim_frame++;
		if (anim_frame >= 60)
			anim_frame = 0;

		switch (rec_a_buffer)
		{
		case 'K':
		case 'k':
			struct tm time;
			tk_get(&time);
			printf("Datum: %04d-%02d-%02d %02d:%02d:%02d\n",
				   time.tm_year + 1900,
				   time.tm_mon + 1,
				   time.tm_mday,
				   time.tm_hour,
				   time.tm_min,
				   time.tm_sec);
			break;

		case 'H':
		case 'h':
			printf("Hallo!\n");
			break;

		case 'D':
		case 'd':
		{
			RTC_HOLD_SET();
			while (RTC_IS_BUSY())
				;

			int d10 = RTC_READ(RTC_REG_D10);
			int d1 = RTC_READ(RTC_REG_D1);
			int mo10 = RTC_READ(RTC_REG_MO10);
			int mo1 = RTC_READ(RTC_REG_MO1);
			int y10 = RTC_READ(RTC_REG_Y10);
			int y1 = RTC_READ(RTC_REG_Y1);

			RTC_HOLD_CLR();

			int day = d10 * 10 + d1;
			int mon = mo10 * 10 + mo1;
			int year = 2000 + y10 * 10 + y1;
			printf("Datum: %02d.%02d.%04d\n", day, mon, year);
			break;
		}

		case 'T':
		case 't':
		{
			RTC_HOLD_SET();
			while (RTC_IS_BUSY())
				;

			int s10 = RTC_READ(RTC_REG_S10);
			int s1 = RTC_READ(RTC_REG_S1);
			int mi10 = RTC_READ(RTC_REG_MI10);
			int mi1 = RTC_READ(RTC_REG_MI1);
			int h10 = RTC_READ(RTC_REG_H10) & 0x3;
			int h1 = RTC_READ(RTC_REG_H1);

			RTC_HOLD_CLR();

			int sec = s10 * 10 + s1;
			int min = mi10 * 10 + mi1;
			int hour = h10 * 10 + h1;
			printf("Uhrzeit: %02d:%02d:%02d\n", hour, min, sec);
			break;
		}

		default:
			if (rec_a_buffer >= '0' && rec_a_buffer <= '7')
			{
				dump_input(rec_a_buffer - '0');
			}
			else if (rec_a_buffer != 0)
			{
				char buf[2] = {rec_a_buffer, '\0'};
				print_string(buf);
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
			hd63484_draw_string(8, 8, "Das waren 30000 Umdrehungen.");
			counter = 0;
		}
	} while (counter < 60000);
	return 0;
}
