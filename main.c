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
#include "font.h"   /* font8x8[128][8] */
#include "pt3player.h"

#include "io.h"

/* Music data from music_data.c */
extern unsigned char pt3player_main_track_pt3[];
extern unsigned int pt3player_main_track_pt3_len;

/* ---- Screen geometry ---- */
#define SCREEN_W    384
#define SCREEN_H    280

/*
 * Horizontal timing (memory cycles):
 *   HC  = total cycles - 1 = 383  (384 total = SCREEN_W)
 *   HSW = 2  (minimum valid sync width)
 *   HDS = 0  (display starts 1 cycle after HSYNC rise, encoded as HS-1=0)
 *   HDW = 95 (display width: (95+1)*4px = 384px = SCREEN_W)
 *
 * Vertical timing (rasters):
 *   VC  = 280  (total rasters = SCREEN_H, stored directly not -1)
 *   VSW = 2    (sync width)
 *   VDS = 0    (display starts at raster 1, encoded as VS-1=0)
 *   SP1 = 280  (Base screen height = full SCREEN_H rasters)
 *
 * Memory width = SCREEN_W / 4 px_per_word = 96 words per line
 */
#define HTOTAL         384
#define HSYNC_W          2
#define HDISP_S          0
#define HDISP_W         95

#define VTOTAL         280
#define VSYNC_W          2
#define VDISP_S          0
#define SCREEN_RASTERS 280

#define MEM_WIDTH       96
#define VRAM_BASE   0x00000UL

/* ---- 4bpp IRGB palette indices ---- */
#define PAL_BLACK    0x0u
#define PAL_RED      0x1u
#define PAL_GREEN    0x2u
#define PAL_YELLOW   0x3u
#define PAL_BLUE     0x4u
#define PAL_MAGENTA  0x5u
#define PAL_CYAN     0x6u
#define PAL_WHITE    0x7u
#define PAL_GRAY     0x8u
#define PAL_BRED     0x9u
#define PAL_BGREEN   0xAu
#define PAL_BYELLOW  0xBu
#define PAL_BBLUE    0xCu
#define PAL_BMAGENTA 0xDu
#define PAL_BCYAN    0xEu
#define PAL_BWHITE   0xFu

#define BTN_UP      (1u << 1)
#define BTN_RIGHT   (1u << 4)
#define BTN_DOWN    (1u << 7)
#define BTN_LEFT    (1u << 10)

#include <stdio.h>

// ISR - see platform.ld
void __attribute__((interrupt))
vector64(void) {
	;
}

void display_inputs()
{
        scan_inputs();

        for (int n = 0; n < 8; n++)
        {
            uint16_t input_state = read_input(n);
            // Draw label "Input n: 0xXXXX"
            char label[20];
            sprintf(label, "Input %d: 0x%04X", n, input_state);
            hd63484_draw_string(8, 180 + n*10, label, PAL_WHITE, PAL_BLACK);
        }
        char hex_str[5];
        hd63484_draw_string(150, 180, "YM2149A", PAL_WHITE, PAL_BLACK);
        sprintf(hex_str, "%04X", read_ioa());
        hd63484_draw_string(220, 180, hex_str, PAL_WHITE, PAL_BLACK);
        hd63484_draw_string(150, 190, "YM2149B", PAL_WHITE, PAL_BLACK);
        sprintf(hex_str, "%04X", read_iob());
        hd63484_draw_string(220, 190, hex_str, PAL_WHITE, PAL_BLACK);
}

int main(void)
{
    hd63484_config_t cfg;

    cfg.gbm       = GBM_4BPP;
    cfg.ccr_ie    = 0;

    cfg.acm       = ACM_SINGLE;
    cfg.gai       = GAI_INC1;      /* 1 word/cycle = 4 pixels at 4bpp */
    cfg.rsm       = RSM_NONINTERLACE;
    cfg.ram_mode  = 1;             /* static RAM, no DRAM refresh      */
    cfg.acp       = 0;             /* display priority                 */

    cfg.dcr       = DCR_DSP | DCR_SE1;  /* Base screen on, standard DISP */

    cfg.hc        = HTOTAL;
    cfg.hsw       = HSYNC_W;
    cfg.hds       = HDISP_S;
    cfg.hdw       = HDISP_W;

    cfg.vc        = VTOTAL;
    cfg.vsw       = VSYNC_W;
    cfg.vds       = VDISP_S;
    cfg.sp1       = SCREEN_RASTERS;

    cfg.mwr1      = MEM_WIDTH;
    cfg.sar1      = VRAM_BASE;
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
    hd63484_draw_string(8, 8, "Testprogramm fur STELLA Gerate.", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 16, "Und andere Banditen", PAL_WHITE, PAL_BLUE);

    setup_duart();

    printf("Hello, world!\n");

    /* Initialize PT3 player */
    func_setup_music(pt3player_main_track_pt3, pt3player_main_track_pt3_len, 0, 0);

    char recv;

    uint16_t counter = 0;

    do {
        func_play_tick(0);
        audio_update_pt3();

        recv = getchar_();
        if (recv != 0)
        {
            // putchar_(recv); // Echo back the received character
            char ser_str[5];
            sprintf(ser_str, "%04X", read_iob());
            hd63484_draw_string(200, 180, ser_str, PAL_YELLOW, PAL_BLACK);
        }
        counter++;
        if (counter % 10 == 0)
        {
            display_inputs();
        }
        if (counter == 30000)
        {
            // Change the text color after 30000 iterations
            hd63484_draw_string(8, 8, "Es wurde ein Problem festgestellt.", PAL_YELLOW, PAL_BLUE);
            counter = 0;
        }
    } while (counter < 60000);
    return 0;
}