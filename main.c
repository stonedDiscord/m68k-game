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

#include "gpu.h"
#include "font.h"   /* font8x8[128][8] */

#include "io.h"

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

    hd63484_set_font(font8x8);

    /* 1. Clear to black */
    hd63484_clear_screen(PAL_BLUE, SCREEN_W, SCREEN_H);

    /* 2. White border */
    hd63484_set_color1(PAL_WHITE);
    hd63484_amove(0, 0);
    hd63484_arct(SCREEN_W - 1, SCREEN_H - 1, AREA_NONE, COL_REG_IND, OPM_REPLACE);

    /* 4. Cyan diagonal line */
    hd63484_draw_line(0, 0, SCREEN_W - 1, SCREEN_H - 1, PAL_CYAN);
    hd63484_draw_line(0, SCREEN_H - 1, SCREEN_W - 1, 0, PAL_MAGENTA);

    /* Text on a coloured background */
    hd63484_draw_string(8, 8, "Es wurde ein Problem festgestellt.", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 16, "Windows wurde heruntergefahren, damit der", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 24, "Computer nicht beschadigt wird.", PAL_WHITE, PAL_BLUE);

    hd63484_draw_string(8, 40, "Wenn Sie diese Fehlermeldung zum ersten Mal", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 48, "angezeigt bekommen, sollten Sie den Computer", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 56, "neu starten. Wenn diese Meldung weiterhin", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 64, "angezeigt wird, mussen Sie folgenden", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 72, "Schritten folgen:", PAL_WHITE, PAL_BLUE);

    hd63484_draw_string(8, 80, "Uberprufen Sie den Computer auf Viren. Entfernen Sie alle", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 88, "neu installierten Festplatten bzw. Festplattencontroller.", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 96, "Stellen Sie sicher, dass die Festplatte richtig konfiguriert", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 104, "und beendet ist. Fuhren Sie CHKDSK /F aus, um festzustellen,", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 112, "ob die Festplatte beschadigt ist, und starten Sie anschliessend", PAL_WHITE, PAL_BLUE);
    hd63484_draw_string(8, 120, "den Computer erneut.", PAL_WHITE, PAL_BLUE);

    hd63484_draw_string(8, 144, "Technische Information:", PAL_WHITE, PAL_BLUE);

    hd63484_draw_string(8, 152, "*** STOP: 0x0000007B", PAL_WHITE, PAL_RED);
    hd63484_draw_string(8, 160, "INACCESSIBLE_BOOT_DEVICE", PAL_WHITE, PAL_RED);

    //setup_duart();
    //putchar_("E");

    uint16_t input_state;

    for (;;) {
        scan_inputs();
        uint16_t input_state = read_input(0);
        hd63484_draw_string(8, 180, "Input state: 0x", PAL_WHITE, PAL_BLACK);
        char hex_str[5];
        for (int i = 0; i < 4; i++) {
            uint8_t nibble = (input_state >> (12 - 4*i)) & 0xF;
            hex_str[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        }
        hex_str[4] = '\0';
        hd63484_draw_string(8 + 16*8, 180, hex_str, PAL_YELLOW, PAL_BLACK);
    }
    return 0;
}
