/*
 * main.c – Profitech 3000 Homebrew
 *
 * Configured to match the ADP/Merkur arcade hardware as emulated in MAME
 * (adp.cpp driver: quickjac, skattv, skattva, fashiong etc.)
 *
 * System specs:
 *   CPU  : 68EC000 @ 8 or 12 MHz (CPU_MHZ build setting)
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

/* ISR producer, main-loop consumer; byte indices wrap at 256. */
static volatile char serial_rx[256];
static volatile uint8_t serial_rx_head;
static volatile uint8_t serial_rx_tail;
static volatile bool serial_rx_overflow;

static char memory_command;
static char memory_args[32];
static uint8_t memory_args_length;
static bool memory_args_overflow;

static void serial_memory_start(char command)
{
	memory_command = command;
	memory_args_length = 0;
	memory_args_overflow = false;
}

/* Read an unsigned hex argument, rejecting signs, overflow and junk. */
static bool serial_hex(const char **text, uint32_t limit, uint32_t *value)
{
	const char *p = *text;
	uint32_t number = 0;
	bool have_digit = false;
	while (*p == ' ' || *p == '\t')
		p++;
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;
	while (*p && *p != ' ' && *p != '\t')
	{
		unsigned digit;
		if (*p >= '0' && *p <= '9')
			digit = *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			digit = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			digit = *p - 'A' + 10;
		else
			return false;
		if (number > (limit - digit) / 16)
			return false;
		number = number * 16 + digit;
		have_digit = true;
		p++;
	}
	while (*p == ' ' || *p == '\t')
		p++;
	*text = p;
	*value = number;
	return have_digit;
}

/* Consume arguments without blocking the display or timer-driven music. */
static bool serial_memory_input(char c)
{
	if (!memory_command)
		return false;
	if (c == '\033')
	{
		memory_command = 0;
		return true;
	}
	if (c == '\r' || c == '\n')
	{
		uint32_t address, value = 0;
		const char *args = memory_args;
		char command = memory_command;
		memory_command = 0;
		memory_args[memory_args_length] = '\0';
		if (memory_args_overflow ||
			!serial_hex(&args, 0xffffff, &address) ||
			(command == 'w' && !serial_hex(&args, 0xff, &value)) || *args)
		{
			printf("Usage: p <hex-address> | w <hex-address> <hex-byte>\n");
			return true;
		}
		volatile uint8_t *memory = (volatile uint8_t *)(uintptr_t)address;
		if (command == 'w')
			*memory = (uint8_t)value;
		else
			value = *memory;
		printf("%06lX %c %02X\n", (unsigned long)address,
			   command == 'w' ? '<' : ':', (unsigned)value);
		return true;
	}
	if (c == '\b' || c == '\177')
	{
		if (memory_args_length)
			memory_args_length--;
	}
	else if ((unsigned char)c < ' ' && c != '\t')
		memory_args_overflow = true;
	else if (memory_args_length < sizeof(memory_args) - 1)
		memory_args[memory_args_length++] = c;
	else
		memory_args_overflow = true;
	return true;
}

/* Simple xorshift32 PRNG */
static uint32_t prng_state = 1;

static uint32_t prng_next(void)
{
	uint32_t x = prng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	prng_state = x;
	return x;
}

static void prng_seed(uint32_t seed)
{
	prng_state = seed;
}

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
	static uint8_t music_ticks;

	/* Refresh lamps and digits every millisecond, independently of video. */
	output_test_tick();
	if (++music_ticks < 20)
		return;
	music_ticks = 0;
	/* PT3 playback requires 50 Hz, independent of the CPU oscillator. */
	func_play_tick(0);
	audio_update_pt3();
	timer_ticks++;
	if (timer_ticks >= 50)
	{ /* 50 * 20 ms = 1 s */
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
		uint8_t next = (uint8_t)(serial_rx_head + 1);
		if (next != serial_rx_tail)
		{
			serial_rx[serial_rx_head] = c;
			serial_rx_head = next;
		}
		else
			serial_rx_overflow = true;
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

void gesicht_bg()
{
	hd63484_set_color_bg(PAL_BWHITE);
	hd63484_set_color_fg(PAL_BWHITE);
	hd63484_set_edge_color(PAL_BWHITE);
	hd63484_amove(320, 64);
	hd63484_crcl(32, 0, AREA_NONE, COL_DIRECT, OPM_REPLACE);
	hd63484_set_edge_color(PAL_BWHITE);
	hd63484_paint(1, AREA_NONE, COL_REG_IND, OPM_REPLACE);
}

void sonne()
{
	const char sternenfarben = PAL_BYELLOW;
	hd63484_draw_line(330, 1, 331, 26, sternenfarben);
	hd63484_draw_line(331, 26, 348, 11, sternenfarben);
	hd63484_draw_line(348, 11, 341, 37, sternenfarben);
	hd63484_draw_line(341, 37, 369, 25, sternenfarben);
	hd63484_draw_line(369, 25, 351, 49, sternenfarben);
	hd63484_draw_line(351, 49, 381, 50, sternenfarben);
	hd63484_draw_line(381, 50, 354, 69, sternenfarben);
	hd63484_draw_line(354, 69, 378, 78, sternenfarben);
	hd63484_draw_line(378, 78, 353, 78, sternenfarben);
	hd63484_draw_line(353, 78, 364, 107, sternenfarben);
	hd63484_draw_line(364, 107, 340, 91, sternenfarben);
	hd63484_draw_line(340, 91, 342, 123, sternenfarben);
	hd63484_draw_line(342, 123, 325, 102, sternenfarben);
	hd63484_draw_line(325, 102, 318, 122, sternenfarben);
	hd63484_draw_line(318, 122, 311, 102, sternenfarben);
	hd63484_draw_line(311, 102, 294, 121, sternenfarben);
	hd63484_draw_line(294, 121, 299, 91, sternenfarben);
	hd63484_draw_line(299, 91, 271, 105, sternenfarben);
	hd63484_draw_line(271, 106, 289, 82, sternenfarben);
	hd63484_draw_line(289, 82, 261, 84, sternenfarben);
	hd63484_draw_line(261, 84, 282, 72, sternenfarben);
	hd63484_draw_line(282, 72, 257, 63, sternenfarben);
	hd63484_draw_line(257, 63, 286, 57, sternenfarben);
	hd63484_draw_line(286, 57, 265, 36, sternenfarben);
	hd63484_draw_line(265, 36, 288, 41, sternenfarben);
	hd63484_draw_line(288, 41, 277, 15, sternenfarben);
	hd63484_draw_line(277, 15, 302, 33, sternenfarben);
	hd63484_draw_line(302, 33, 300, 6, sternenfarben);
	hd63484_draw_line(300, 6, 315, 28, sternenfarben);
	hd63484_draw_line(315, 28, 330, 1, sternenfarben);

	hd63484_amove(340, 25);
	hd63484_set_color_fg(sternenfarben);
	hd63484_set_edge_color(sternenfarben);
	hd63484_paint(1, AREA_NONE, COL_REG_IND, OPM_REPLACE);
}

void gesicht()
{
	hd63484_set_color_bg(PAL_BLACK);
	hd63484_set_edge_color(PAL_BLACK);
	hd63484_amove(298, 59);
	hd63484_elps(10, 10, 5, 10, 0, AREA_NONE, COL_REG_IND, OPM_REPLACE);
	hd63484_amove(335, 60);
	hd63484_elps(10, 10, 5, 10, 0, AREA_NONE, COL_REG_IND, OPM_REPLACE);
}

void run_tests()
{
	/* Text on a coloured background */
	hd63484_set_color_bg(PAL_BLACK);
	hd63484_set_color_fg(PAL_WHITE);
	println("Testprogramm:");
	printf("Testprogramm\n\n");
	printf("SuS CPU: %d MHz\n", CPU_MHZ);
	println(" ");

	/* Test 1: RAMDAC */
	print_string("Test 1 RAMDAC: ");
	printf("Test 1 RAMDAC: ");
	if (ramdac_read_mask() == 0xFF)
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
		.tm_sec = 30,	// seconds [0, 60]
		.tm_min = 46,	// minutes [0, 59]
		.tm_hour = 8,	// hours [0, 23]
		.tm_mday = 11,	// day of month [1, 31]
		.tm_mon = 8,	// months since January [0, 11]
		.tm_year = 101, // years since 1900 (2024 -> 124)
		.tm_wday = 2,	// day of week [0, 6] (Sunday = 0)
		.tm_yday = 254, // day of year [0, 365]
		.tm_isdst = 0	// daylight saving time unknown
	};

	/* Test 2: RTC */
	print_string("Test 2 RTC: ");
	printf("Test 2 RTC: ");
	struct tm time_rtc;
	if (rtc_get(&time_rtc) != 0)
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
	printf("Test 4 DUART (Hallo!): ");
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
	print_string("Test 5 Video (Hallo!): ");
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
}

int main(void)
{
	setup_duart();

	/* The timer ISR uses the player state: initialise it before unmasking IRQs.
	 * Start audio before any optional video, serial or clock diagnostics. */
	func_setup_music(pt3player_main_track_pt3, pt3player_main_track_pt3_len, 0, 0);
	enable_interrupts();

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

	run_tests();

	/* Seed PRNG with current time */
	struct tm seed_time;
	if (tk_get(&seed_time) == 0)
	{
		uint32_t seed = (seed_time.tm_sec + 1) * (seed_time.tm_min + 1) *
						(seed_time.tm_hour + 1) * (seed_time.tm_mday + 1);
		prng_seed(seed);
	}
	else
	{
		prng_seed(timer_ticks ^ 0xDEADBEEF);
	}

	uint16_t counter = 0;

	uint8_t anim_frame = 0;

	gesicht_bg();
	sonne();
	// gesicht();

	do
	{
		static bool serial_discard_line;
		while (serial_rx_tail != serial_rx_head)
		{
			if (serial_rx_overflow)
			{
				serial_rx_overflow = false;
				serial_rx_tail = serial_rx_head;
				memory_command = 0;
				serial_discard_line = true;
				printf("Serial receive overflow; resend command after Enter\n");
				break;
			}
			rec_a_buffer = serial_rx[serial_rx_tail];
			serial_rx_tail = (uint8_t)(serial_rx_tail + 1);
			if (serial_discard_line)
			{
				if (rec_a_buffer == '\r' || rec_a_buffer == '\n')
					serial_discard_line = false;
				continue;
			}
			if (serial_memory_input(rec_a_buffer))
				continue;
			switch (rec_a_buffer)
			{
			case 'P':
			case 'p':
				serial_memory_start('p');
				break;

			case 'W':
			case 'w':
				serial_memory_start('w');
				break;

			case '\r':
			case '\n':
				break;

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

		}
		rec_a_buffer = 0x00;

		counter++;
		if (counter % 10 == 0)
		{
			display_all_inputs();
		}
		if (counter == 5000)
		{
			print_string("Test IPv7: ");
			printf("Test IPv7: ");
			{
				hd63484_set_color_bg(PAL_RED);
				println("Nein");
				printf("Nein\n");
			}
			hd63484_set_color_bg(PAL_BLACK);
		}
		if (counter >= 10000 && counter < 20000 && counter % 10 == 0)
		{
			hd63484_set_color_bg(PAL_BLUE);
			hd63484_set_color_fg(PAL_WHITE);
			uint16_t x = prng_next() % SCREEN_W;
			uint16_t y = prng_next() % SCREEN_H;
			hd63484_draw_string(x, y, "lain");
		}
		if (counter == 21000)
		{
			print_string("Test 8 Wired: ");
			printf("Test 8 Wired: ");
			{
				hd63484_set_color_bg(PAL_GREEN);
				println("OK");
				printf("OK\n");
			}
			hd63484_set_color_bg(PAL_BLACK);
		}
		if (counter == 30000)
		{
			{
				print_string("Test 9 Lain: ");
				printf("Test 9 Lain: ");
				{
					hd63484_set_color_bg(PAL_RED);
					println("Nein");
					printf("Nein\n");
				}
				hd63484_set_color_bg(PAL_BLACK);
			}
		}
		if (counter == 35000)
		{
			gesicht_bg();
			sonne();
		}
		if (counter >= 40000)
		{
			stringx = 0;
			stringy = 0;
			run_tests();
		}
		if (counter >= 60000)
		{
			counter = 0;
		}
	} while (counter < 60000);
	return 0;
}
