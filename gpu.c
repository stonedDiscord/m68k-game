/*
 * gpu.c – HD63484 ACRTC Driver Library
 *
 * Hitachi HD63484 Advanced CRT Controller
 * For M68K homebrew systems
 *
 * See gpu.h for full documentation and register/command reference.
 */

#include "gpu.h"
#include "font.h"
#include <stdbool.h>

/* Screen height stored at init time; used by hd63484_sy() for Y-flipping */
int16_t hd63484_screen_height = 0;

/* ---------------------------------------------------------------------------
 * Platform register pointers  (defined here; declared extern in gpu.h)
 * Adjust base addresses for your board's address decoding.
 * ---------------------------------------------------------------------------*/
volatile uint16_t *hd63484_addr = (volatile uint16_t *)0x800080;    /* RS=0 */
volatile uint16_t *hd63484_control = (volatile uint16_t *)0x800082; /* RS=1 */

volatile uint8_t *ramdac_index = (volatile uint8_t *)0x800089;
volatile uint8_t *ramdac_palette = (volatile uint8_t *)0x80008b;
volatile uint8_t *ramdac_mask = (volatile uint8_t *)0x80008d;

/* ===========================================================================
 * Low-level bus helpers
 * ===========================================================================*/

/*
 * hd63484_write_ar – write a register address to the Address Register.
 * The AR is at RS=0 on a write cycle.
 *
 * The HD63484 reads the register address from the LOW byte of the 16-bit
 * word written to RS=0.  Do NOT shift the address into the high byte.
 */
void hd63484_write_ar(uint8_t reg_addr)
{
    *hd63484_addr = (uint16_t)reg_addr;
}

/*
 * hd63484_read_sr – read the Status Register (RS=0, R/W=1).
 * Returned as a full 16-bit word; only bits 7–0 are meaningful.
 */
uint16_t hd63484_read_sr(void)
{
    return *hd63484_addr;
}

/*
 * hd63484_write_reg – point AR at reg_addr, then write a 16-bit value.
 * For addresses r80–rFF the chip auto-increments AR after each access,
 * enabling fast sequential block writes without reloading AR.
 */
void hd63484_write_reg(uint8_t reg_addr, uint16_t value)
{
    hd63484_write_ar(reg_addr);
    *hd63484_control = value;
}

/*
 * hd63484_read_reg – point AR at reg_addr, then read back the 16-bit value.
 */
uint16_t hd63484_read_reg(uint8_t reg_addr)
{
    hd63484_write_ar(reg_addr);
    return *hd63484_control;
}

/* ===========================================================================
 * FIFO helpers
 *
 * All commands and drawing parameters are funnelled through the write FIFO
 * (16 bytes deep = 8 words).  AR must contain 0x00 (REG_FE) before any
 * FIFO access; we set it once before each burst.
 * ===========================================================================*/

void hd63484_wait_wfr(void)
{
    while (!(hd63484_read_sr() & SR_WFR))
        ;
}

void hd63484_wait_wfe(void)
{
    while (!(hd63484_read_sr() & SR_WFE))
        ;
}

void hd63484_wait_rfr(void)
{
    while (!(hd63484_read_sr() & SR_RFR))
        ;
}

void hd63484_wait_ced(void)
{
    while (!(hd63484_read_sr() & SR_CED))
        ;
}

/*
 * hd63484_fifo_write – send one 16-bit word to the write FIFO.
 * Caller is responsible for first selecting REG_FE via write_ar,
 * or using hd63484_wait_wfr() before each word if needed.
 */
void hd63484_fifo_write(uint16_t word)
{
    // hd63484_wait_wfr();
    *hd63484_control = word;
}

/*
 * hd63484_fifo_read – read one 16-bit word from the read FIFO.
 */
uint16_t hd63484_fifo_read(void)
{
    hd63484_wait_rfr();
    return *hd63484_control;
}

/* Internal helper: select FIFO entry and write a word */
static inline void fifo_w(uint16_t word)
{
    // hd63484_wait_wfr();
    *hd63484_control = word;
}

/* ===========================================================================
 * Chip-level control
 * ===========================================================================*/

/*
 * hd63484_reset – software reset via CCR ABT.
 *
 * The HD63484 has a hardware RES pin that is the true reset mechanism;
 * that is handled by your board's power-on circuitry and is not accessible
 * as a register write.
 *
 * The software equivalent is the ABT (abort) bit in CCR which:
 *   - Stops the current drawing command
 *   - Clears both read and write FIFOs
 *   - Sets SR to 0x23 (WFE=1, WFR=1, CED=1)
 *
 * After ABT we immediately clear it so the chip can accept new commands.
 *
 * NOTE: The original stub wrote 0x4400 to the data port (RS=1) with AR=0,
 * which sent 0x4400 as a FIFO command — not a reset.  That line is removed.
 */
void hd63484_reset(void)
{
    /* Assert ABT: write CCR with ABT=1 via direct register access */
    hd63484_write_ar(REG_CCR);
    *hd63484_control = CCR_ABT;

    /* Wait until WFE is set — FIFOs are confirmed empty */
    // hd63484_wait_wfe();

    /* Clear ABT so commands can flow; all interrupt enables off */
    hd63484_write_ar(REG_CCR);
    *hd63484_control = 0x0000;
}

/*
 * hd63484_abort – set ABT to flush FIFOs and stop any running command.
 * After this you should call hd63484_write_reg(REG_CCR, 0) to clear ABT
 * when ready to continue.
 */
void hd63484_abort(void)
{
    hd63484_write_reg(REG_CCR, CCR_ABT);
    /* Wait until WFE set (FIFOs cleared) */
    // hd63484_wait_wfe();
}

/*
 * hd63484_start – set the STR bit in OMR to start display scanning.
 * Call this only after all timing and display registers are programmed.
 * Reads OMR first to preserve other bits.
 */
void hd63484_start(void)
{
    uint16_t omr = hd63484_read_reg(REG_OMR);
    hd63484_write_reg(REG_OMR, omr | OMR_STR);
}

/*
 * hd63484_stop – clear STR to halt the display.
 */
void hd63484_stop(void)
{
    uint16_t omr = hd63484_read_reg(REG_OMR);
    hd63484_write_reg(REG_OMR, omr & (uint16_t)~OMR_STR);
}

/*
 * hd63484_disable_screen – zero out MWR for one screen so the chip
 * skips it entirely during display.  Safe to call after hd63484_init().
 *
 * screen: SCREEN_UPPER(0), SCREEN_BASE(1), SCREEN_LOWER(2), SCREEN_WINDOW(3)
 *
 * We only need to zero MWR — the chip treats MWR=0 as "screen disabled"
 * regardless of SAR.  We use a targeted write rather than a block write
 * so we don't disturb adjacent screens.
 *
 * MWR register addresses:
 *   Screen 0: 0xC2   Screen 1: 0xCA   Screen 2: 0xD2   Screen 3: 0xDA
 */
void hd63484_disable_screen(uint8_t screen)
{
    static const uint8_t mwr_addrs[4] = {
        REG_MWR0, REG_MWR1, REG_MWR2, REG_MWR3
    };
    if (screen > 3u) return;
    hd63484_write_reg(mwr_addrs[screen], 0x0000u);
}

/*
 * hd63484_enable_screen – (re)point a screen at a valid VRAM address.
 *
 * Writes RAR=0 (graphic mode), MWR=mem_width, SAR high+low.
 * Use this if you need to swap which screen is active at runtime,
 * e.g. after detecting which half of the address space is RAM.
 *
 * vram_addr: 20-bit word address (same format as VRAM_LOWER/VRAM_UPPER)
 * mem_width: words per line, e.g. MEM_WIDTH (96)
 */
void hd63484_enable_screen(uint8_t screen, uint32_t vram_addr, uint16_t mem_width)
{
    static const uint8_t rar_addrs[4] = {
        REG_RAR0, REG_RAR1, REG_RAR2, REG_RAR3
    };
    if (screen > 3u) return;

    hd63484_write_ar(rar_addrs[screen]);           /* auto-inc from RAR */
    *hd63484_control = 0x0000u;                    /* RAR: graphic mode */
    *hd63484_control = mem_width & 0x03FFu;        /* MWR              */
    *hd63484_control = (uint16_t)((vram_addr >> 16) & 0x000Fu); /* SARH */
    *hd63484_control = (uint16_t)(vram_addr & 0xFFFFu);         /* SARL */
}

/* ===========================================================================
 * Full initialisation
 *
 * Sequence (from the manual section 4 / 5):
 *  1. Hardware reset
 *  2. Program CCR  (GBM, interrupt enables) – STR must still be 0
 *  3. Program OMR  (access mode, GAI, RSM, RAM mode, ACP) – STR still 0
 *  4. Program DCR  (which screens are enabled)
 *  5. Program Timing Control RAM (r80–r9F) via block write
 *  6. Program Display Control RAM (rC0–rEF) – start addresses, widths
 *  7. Set ORG and initial drawing parameters via FIFO commands
 *  8. Set STR=1 to start
 * ===========================================================================*/
void hd63484_init(void)
{
    /* ---- 1. Reset ---- */
    hd63484_reset();

    /* Store screen height for Y-coordinate flipping in drawing wrappers */
    hd63484_screen_height = (int16_t)SCREEN_RASTERS;

    /* ---- 2. CCR ---- */
    {
        uint16_t ccr = 0;
        ccr |= (uint16_t)(GBM_4BPP & 0x7u) << CCR_GBM_SHIFT;
        ccr |= 0 & 0x00FFu; /* interrupt enables in low byte: none */
        hd63484_write_reg(REG_CCR, ccr);
    }

    /* ---- 3. OMR ---- */
    {
        uint16_t omr = 0;
        omr |= OMR_MIS;                              /* single chip = master */
        omr |= (uint16_t)(0u) << 13;                 /* ACP: display priority */
        /* CSK=01, DSK=01 (1 cycle skew) – safe default for most systems */
        omr |= (1u << OMR_CSK_SHIFT);
        omr |= (1u << OMR_DSK_SHIFT);
        omr |= (uint16_t)(1u) << 7;                  /* RAM: SRAM mode */
        omr |= (uint16_t)(GAI_INC1 & 0x7u) << OMR_GAI_SHIFT;
        omr |= (uint16_t)(ACM_SINGLE & 0x3u) << OMR_ACM_SHIFT;
        omr |= (uint16_t)(RSM_NONINTERLACE & 0x3u);
        /* NOTE: STR is NOT set here – must be last */
        hd63484_write_reg(REG_OMR, omr);
    }

    /* ---- 4. DCR ---- */
    hd63484_write_reg(REG_DCR, DCR_DSP | DCR_SE1); /* Base screen on */

    /* ---- 5. Timing Control RAM (r80–r9F, block write) ----
     *
     * Point AR at r80; subsequent writes auto-increment AR.
     * Registers written in order: RCR r80, HSR r82, HDR r84,
     * VSR r86, VDR r88, SSW r8A r8C r8E, BCR r90, HWR r92,
     * VWR r94 r96, GCR r98 (total 16 word-registers = 32 bytes).
     *
     * For registers we don't configure here we write 0.
     */
    hd63484_write_ar(REG_RCR); /* = r80, auto-inc from here */

    /* r80–r81  RCR (read-only, but we must clock past it) */
    *hd63484_control = 0x0000;

    /* r82–r83  HSR: HC in high byte, HSW in low bits */
    *hd63484_control = ((uint16_t)HTOTAL << 8) |
                       (HSYNC_W & 0x1Fu);

    /* r84–r85  HDR: HDS in high byte, HDW in low byte */
    *hd63484_control = ((uint16_t)HDISP_S << 8) |
                       HDISP_W;

    /* r86–r87  VSR: VC (12-bit vertical cycle) */
    *hd63484_control = VTOTAL & 0x0FFFu;

    /* r88–r89  VDR: VDS in high byte (bits 15–8), VSW in low bits (4–0) */
    *hd63484_control = ((uint16_t)VDISP_S << 8) |
                       (VSYNC_W & 0x1Fu);

    /* r8A–r8B  SSW Base width (SP1) */
    *hd63484_control = SCREEN_RASTERS & 0x0FFFu;

    /* r8C–r8D  SSW Upper width (SP0) – disabled, write 0 */
    *hd63484_control = 0x0000;

    /* r8E–r8F  SSW Lower width (SP2) – disabled, write 0 */
    *hd63484_control = 0x0000;

    /* r90–r91  BCR – blink off */
    *hd63484_control = 0x0000;

    /* r92–r93  HWR – window disabled, write 0 */
    *hd63484_control = 0x0000;

    /* r94–r95  VWR start – 0 */
    *hd63484_control = 0x0000;

    /* r96–r97  VWR width – 0 */
    *hd63484_control = 0x0000;

    /* r98–r9F  GCR + work area – leave as 0 */
    *hd63484_control = 0x0000;
    *hd63484_control = 0x0000;
    *hd63484_control = 0x0000;
    *hd63484_control = 0x0000;

    /* ---- 6. Display Control RAM (rC0–rDE, block write) ----
     *
     * Each screen occupies 4 consecutive words (auto-increment steps by 2
     * in 16-bit mode):
     *   +0 = RAR  (raster address — 0 for pure graphic screens)
     *   +2 = MWR  (memory width in words, CHR=0 for graphic)
     *   +4 = SARH (start address bits 19–16 in bits 3–0)
     *   +6 = SARL (start address bits 15–0)
     *
     * We write all four screens contiguously starting at rC0.
     * Screens 0, 2, 3 are disabled (zeroed); screen 1 (Base) carries our values.
     */
    hd63484_write_ar(REG_RAR0); /* = 0xC0, auto-inc from here */

    /* Screen 0 (Upper) */
    hd63484_enable_screen(SCREEN_UPPER,VRAM_UPPER,MEM_WIDTH & 0x03FFu);

    /* Screen 1 (Base) - disabled */
    hd63484_disable_screen(SCREEN_BASE);

    /* Screen 2 (Lower) */
    hd63484_enable_screen(SCREEN_LOWER,VRAM_LOWER,MEM_WIDTH & 0x03FFu);

    /* Screen 3 (Window) - disabled, all zero */
    hd63484_disable_screen(SCREEN_WINDOW);

    /* ---- 7. Drawing parameter setup via FIFO ---- */

    /* Select FIFO entry (AR=0x00) */
    hd63484_write_ar(REG_FE);

    /* ORG: associate logical (0,0) with the BOTTOM-LEFT of the frame buffer.
     *
     * MAME's calc_offset computes: physical = org_dpa + x/ppw - y * MWR
     * Y is SUBTRACTED, meaning the coordinate system is Y-UP (Y increases upward).
     * If org_dpa=0 then any y>0 produces a negative (underflowing) address, landing
     * in unmapped VRAM and generating one logerror() call per pixel — which is what
     * causes the apparent "hang" during a full-screen clear.
     *
     * Correct origin: org_dpa = SAR1 + (SCREEN_HEIGHT - 1) * MWR1
     * This maps logical (0, 0)        -> VRAM row (H-1) = bottom of display
     *                 (0, H-1)        -> VRAM row 0     = top of display
     * All addresses stay within [SAR1 .. SAR1 + (H-1)*MWR1] = valid VRAM.
     *
     * Consequence for user code: Y=0 is the BOTTOM of the screen, Y increases UP.
     * To draw from the top-left corner use y = (SCREEN_HEIGHT - 1).
     * Or use the convenience wrapper hd63484_screen_y() defined in gpu.h.
     */
    {
        uint32_t org_addr = VRAM_LOWER +
                            (uint32_t)(SCREEN_RASTERS - 1) * (uint32_t)MEM_WIDTH;
        hd63484_set_origin(0x1 /* DN_BASE */, org_addr, 0);
    }

    /* Drawing pointer: base screen, starting at VRAM_LOWER */
    hd63484_set_dp(0x1, VRAM_LOWER, 0);
    hd63484_set_rwp(0x1, VRAM_LOWER);

    /* Solid white pattern, replace mode */
    hd63484_set_solid_pattern();

    /* Default mask: all bits modifiable */
    hd63484_wpr(PR_MASK, 0xFFFFu);

    /* ---- 8. Start ---- */
    hd63484_start();
}

/* ===========================================================================
 * Drawing Parameter Registers via WPR / RPR FIFO commands
 * ===========================================================================*/

/*
 * hd63484_wpr – Write Parameter Register.
 *
 * Opcode: 0000 1000 0000 RN  (0x0800 | RN)
 * Followed by one 16-bit data word.
 */
void hd63484_wpr(uint8_t pr_addr, uint16_t value)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_WPR | (pr_addr & 0xFFu));
    fifo_w(value);
}

/*
 * hd63484_rpr – Read Parameter Register.
 * Opcode: 0000 1100 0000 RN  (0x0C00 | RN)
 * Result appears in the read FIFO.
 */
uint16_t hd63484_rpr(uint8_t pr_addr)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RPR | (pr_addr & 0xFFu));
    return hd63484_fifo_read();
}

/*
 * hd63484_wptn – Write Pattern RAM.
 *
 * Opcode: 0x1800 | (pra & 0xf)   — pra (0–15) in lower 4 bits of opcode
 * Parameter 0: n  — number of data words to follow
 * Parameters 1..n: data words
 */
void hd63484_wptn(uint8_t pra, uint8_t n, const uint16_t *data)
{
    uint8_t i;
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_WPTN | (pra & 0x0Fu)); /* pra in bits 3:0 of opcode */
    fifo_w((uint16_t)n);              /* first param = count       */
    for (i = 0; i < n; i++)
        fifo_w(data[i]);
}

void hd63484_rptn(uint8_t pra, uint8_t n, uint16_t *data)
{
    uint8_t i;
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RPTN | (pra & 0x0Fu));
    fifo_w((uint16_t)n);
    for (i = 0; i < n; i++)
        data[i] = hd63484_fifo_read();
}

/* ===========================================================================
 * Setup helpers
 * ===========================================================================*/

/*
 * hd63484_set_origin – ORG command.
 *
 * Associates logical (0,0) with a physical frame buffer address and sets the
 * drawing screen (DN). This is the most important init step for drawing:
 * every AMOVE/ALINE/AFRCT coordinate is relative to this origin, and the chip
 * uses the DN field here to look up MWR (memory width) for address arithmetic.
 *
 * If DN is wrong (e.g. points to a disabled screen with MWR=0), the Y-axis
 * contribution to physical address becomes 0 and the chip loops forever trying
 * to fill a rectangle that never advances vertically.
 *
 * Parameters match the Drawing Pointer format (OPH/OPL = DPH/DPL):
 *   dn   : screen number 0–3 (use DN_BASE>>14 = 1 for the Base screen)
 *   addr : 20-bit physical word address in VRAM (usually 0 = start of VRAM)
 *   dot  : sub-word dot position (0 for word-aligned, whole-pixel drawing)
 */
void hd63484_set_origin(uint8_t dn, uint32_t addr, uint8_t dot)
{
    /* 20-bit address encoding (from MAME hd63484.cpp ORG handler):
     *   oph = DN[15:14] | DPAH[7:0]  where DPAH = addr[19:12]
     *   opl = DPAL[15:4] | DPD[3:0] where DPAL = addr[11:0], DPD = dot
     * Decoded by MAME as: dpa = (oph & 0xff)<<12 | (opl & 0xfff0)>>4 */
    uint16_t oph = (uint16_t)((dn & 0x3u) << 14) |
                   (uint16_t)((addr >> 12) & 0xFFu);
    uint16_t opl = (uint16_t)((addr & 0xFFFu) << 4) |
                   (uint16_t)(dot & 0x00Fu);
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_ORG);
    fifo_w(oph);
    fifo_w(opl);
}

void hd63484_set_color0(uint8_t color)
{
    hd63484_wpr(PR_CL0, hd63484_color_reg(color));
}

void hd63484_set_color1(uint8_t color)
{
    hd63484_wpr(PR_CL1, hd63484_color_reg(color));
}

void hd63484_set_edge_color(uint8_t color)
{
    hd63484_wpr(PR_EDG, hd63484_color_reg(color));
}

void hd63484_set_mask(uint16_t mask)
{
    hd63484_wpr(PR_MASK, mask);
}

void hd63484_set_compare_color(uint8_t color)
{
    hd63484_wpr(PR_CCMP, hd63484_color_reg(color));
}

void hd63484_set_area(int16_t xmin, int16_t ymin,
                      int16_t xmax, int16_t ymax)
{
    hd63484_wpr(PR_ADR_XMIN, (uint16_t)xmin);
    hd63484_wpr(PR_ADR_YMIN, (uint16_t)ymin);
    hd63484_wpr(PR_ADR_XMAX, (uint16_t)xmax);
    hd63484_wpr(PR_ADR_YMAX, (uint16_t)ymax);
}

/*
 * hd63484_set_rwp – set the Read/Write Pointer (physical address for data
 * transfer commands).  dn = DN_BASE etc.  addr = 20-bit frame buffer address.
 */
void hd63484_set_rwp(uint8_t dn, uint32_t addr)
{
    uint16_t hi = (uint16_t)((dn & 0x3u) << 14) |
                  (uint16_t)((addr >> 12) & 0xFFu);
    uint16_t lo = (uint16_t)((addr & 0xFFFu) << 4);
    hd63484_wpr(PR_RWP_H, hi);
    hd63484_wpr(PR_RWP_L, lo);
}

/*
 * hd63484_set_dp – set the Drawing Pointer.
 * dot = sub-word pixel address (0–15 for 1bpp, etc.)
 */
void hd63484_set_dp(uint8_t dn, uint32_t addr, uint8_t dot)
{
    uint16_t hi = (uint16_t)((dn & 0x3u) << 14) |
                  (uint16_t)((addr >> 12) & 0xFFu);
    uint16_t lo = (uint16_t)((addr & 0xFFFu) << 4) |
                  (uint16_t)(dot & 0x00Fu);
    hd63484_wpr(PR_DP_H, hi);
    hd63484_wpr(PR_DP_L, lo);
}

/*
 * hd63484_set_solid_pattern – configure the Pattern RAM for a solid fill:
 *   16×16 all-1 pattern, PRC set to full 16×16 extents, no zoom.
 *   CL0=0x0000 (background), CL1=0xFFFF (foreground, overridden by caller).
 */
void hd63484_set_solid_pattern(void)
{
    static const uint16_t solid[16] = {
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    /* Write all 16 words starting at pattern RAM address 0 */
    hd63484_wptn(0, 16, solid);

    /* PRC: full 16×16 extents, no zoom
     * Pr07 layout: [15:12]=PEY [11:8]=PZY [7:4]=PEX [3:0]=PZX
     * PEY=15, PZY=0, PEX=15, PZX=0 -> (15<<12)|(0<<8)|(15<<4)|0 = 0xF0F0 */
    hd63484_wpr(PR_PRC0, 0x0000u); /* PPX=0, PPY=0, PZCX=0, PZCY=0 */
    hd63484_wpr(PR_PRC1, 0x0000u); /* PSX=0, PSY=0                  */
    hd63484_wpr(PR_PRC2, 0xF0F0u); /* PEY=15, PEX=15, no zoom       */
}

/* ===========================================================================
 * Graphic Drawing Commands
 * ===========================================================================*/

/* Internal helper: build the lower byte from area/col/opm sub-fields */
static inline uint16_t draw_flags(uint8_t area, uint8_t col, uint8_t opm)
{
    return ((uint16_t)(area & 0xE0u)) |
           ((uint16_t)(col & 0x18u)) |
           ((uint16_t)(opm & 0x07u));
}

void hd63484_amove(int16_t x, int16_t y)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_AMOVE);
    fifo_w((uint16_t)x);
    fifo_w((uint16_t)hd63484_sy(y));
}

void hd63484_rmove(int16_t dx, int16_t dy)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RMOVE);
    fifo_w((uint16_t)dx);
    fifo_w((uint16_t)(-dy)); /* flip delta too: down in screen = up in chip */
}

void hd63484_aline(int16_t x, int16_t y,
                   uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_ALINE | draw_flags(area, col, opm));
    fifo_w((uint16_t)x);
    fifo_w((uint16_t)hd63484_sy(y));
}

void hd63484_rline(int16_t dx, int16_t dy,
                   uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RLINE | draw_flags(area, col, opm));
    fifo_w((uint16_t)dx);
    fifo_w((uint16_t)(-dy));
}

void hd63484_arct(int16_t x, int16_t y,
                  uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_ARCT | draw_flags(area, col, opm));
    fifo_w((uint16_t)x);
    fifo_w((uint16_t)hd63484_sy(y));
}

void hd63484_rrct(int16_t dx, int16_t dy,
                  uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RRCT | draw_flags(area, col, opm));
    fifo_w((uint16_t)dx);
    fifo_w((uint16_t)(-dy));
}

void hd63484_afrct(int16_t x, int16_t y,
                   uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_AFRCT | draw_flags(area, col, opm));
    fifo_w((uint16_t)x);
    fifo_w((uint16_t)hd63484_sy(y));
}

void hd63484_rfrct(int16_t dx, int16_t dy,
                   uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RFRCT | draw_flags(area, col, opm));
    fifo_w((uint16_t)dx);
    fifo_w((uint16_t)(-dy));
}

/*
 * hd63484_crcl – draw a circle.
 * clockwise: non-zero = clockwise (CRCL_CW bit set in opcode bit 8).
 */
void hd63484_crcl(uint16_t radius, int clockwise,
                  uint8_t area, uint8_t col, uint8_t opm)
{
    uint16_t op = CMD_CRCL | draw_flags(area, col, opm);
    if (clockwise)
        op |= CRCL_CW;
    hd63484_write_ar(REG_FE);
    fifo_w(op);
    fifo_w(radius);
}

/*
 * hd63484_elps – draw an ellipse.
 * a, b = semi-major/minor axis lengths.
 * dx, dy = offset of ellipse centre from current pointer.
 */
void hd63484_elps(uint16_t a, uint16_t b,
                  int16_t dx, int16_t dy,
                  int clockwise,
                  uint8_t area, uint8_t col, uint8_t opm)
{
    uint16_t op = CMD_ELPS | draw_flags(area, col, opm);
    if (clockwise)
        op |= CRCL_CW;
    hd63484_write_ar(REG_FE);
    fifo_w(op);
    fifo_w(a);
    fifo_w(b);
    fifo_w((uint16_t)dx);
    fifo_w((uint16_t)dy);
}

void hd63484_dot(uint8_t area, uint8_t col, uint8_t opm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_DOT | draw_flags(area, col, opm));
}

void hd63484_paint(int stop_at_edge,
                   uint8_t area, uint8_t col, uint8_t opm)
{
    /* PAINT uses CL0, CL1, and EDG — NOT the pattern RAM.
     * MAME stops filling when it hits a pixel equal to EDG, CL0, or CL1.
     * CL1 should be the fill color (set by the caller).
     * EDG should be the boundary color (set by the caller via set_edge_color).
     * CL0 is set to 0xFFFF here so it never matches any valid 4bpp pixel,
     * disabling the secondary boundary. If CL0 were left at 0 (black) it would
     * stop the fill immediately when scanning the black background. */
    hd63484_set_color0(0xFF);

    /* Bit 8 of PAINT opcode: E=0 stops at EDG color, E=1 stops at non-EDG */
    uint16_t op = CMD_PAINT | draw_flags(area, col, opm);
    if (!stop_at_edge)
        op |= (1u << 8);
    hd63484_write_ar(REG_FE);
    fifo_w(op);
}

/*
 * hd63484_ptn – Pattern blit.
 * sz_x, sz_y = pattern size in pixels.
 * sl = SL bit (pattern scan direction).
 * so = SO bit (pattern scan origin).
 */
void hd63484_ptn(int16_t sz_x, int16_t sz_y,
                 uint8_t sl, uint8_t so,
                 uint8_t area, uint8_t col, uint8_t opm)
{
    /* PTN opcode: 1101 | 0100 | SL | SO | AREA | COL | OPM */
    uint16_t op = CMD_PTN |
                  ((uint16_t)(sl & 1u) << 9) |
                  ((uint16_t)(so & 1u) << 8) |
                  draw_flags(area, col, opm);
    hd63484_write_ar(REG_FE);
    fifo_w(op);
    /* SZ word: SZy in high byte, SZx in low byte */
    fifo_w(((uint16_t)(sz_y & 0xFFu) << 8) | (sz_x & 0xFFu));
}

/* ===========================================================================
 * Data Transfer Commands
 * ===========================================================================*/

uint16_t hd63484_rd(void)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_RD);
    return hd63484_fifo_read();
}

void hd63484_wt(uint16_t data)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_WT);
    fifo_w(data);
}

void hd63484_mod(uint16_t data, uint8_t mm)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_MOD | (mm & 0x3u));
    fifo_w(data);
}

/*
 * hd63484_clr – clear a rectangular area using the Read/Write Pointer (RWP).
 *
 * MAME command_clr_exec takes 3 parameters: data (fill value), AX (width), AY (height).
 * RWP must be set to the top-left word address before calling.
 */
void hd63484_clr(uint16_t fill, int16_t ax, int16_t ay)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_CLR);
    fifo_w(fill);
    fifo_w((uint16_t)ax);
    fifo_w((uint16_t)ay);
}

/*
 * hd63484_cpy – copy a rectangular area.
 * sah/sal = source address high/low words.
 * ax, ay  = width, height.
 */
void hd63484_cpy(uint16_t sah, uint16_t sal,
                 int16_t ax, int16_t ay)
{
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_CPY);
    fifo_w(sah);
    fifo_w(sal);
    fifo_w((uint16_t)ax);
    fifo_w((uint16_t)ay);
}

/* Polyline – n points (absolute X,Y pairs in xy[]) */
void hd63484_apll(uint8_t n, const int16_t *xy,
                  uint8_t area, uint8_t col, uint8_t opm)
{
    uint8_t i;
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_APLL | draw_flags(area, col, opm));
    fifo_w(n);
    for (i = 0; i < n * 2u; i++)
        fifo_w((uint16_t)xy[i]);
}

/* Polygon – n points (absolute X,Y pairs) */
void hd63484_aplg(uint8_t n, const int16_t *xy,
                  uint8_t area, uint8_t col, uint8_t opm)
{
    uint8_t i;
    hd63484_write_ar(REG_FE);
    fifo_w(CMD_APLG | draw_flags(area, col, opm));
    fifo_w(n);
    for (i = 0; i < n * 2u; i++)
        fifo_w((uint16_t)xy[i]);
}

/* ===========================================================================
 * High-level convenience functions
 * ===========================================================================*/

/*
 * hd63484_clear_screen – flood the entire visible area with one colour.
 *
 * In screen-space: top-left=(0,0), bottom-right=(w-1,h-1).
 * Internally converts to chip Y-up convention.
 */
void hd63484_clear_screen(uint8_t color, int16_t w, int16_t h)
{
    hd63484_set_color0(0x0);
    hd63484_set_color1(color);
    hd63484_set_solid_pattern();
    hd63484_amove(0, h - 1);                                      /* screen bottom-left */
    hd63484_afrct(w - 1, 0, AREA_NONE, COL_REG_IND, OPM_REPLACE); /* to top-right */
}

/*
 * hd63484_fill_rect – draw a filled rectangle.
 * (x,y) = top-left corner in screen-space; (w,h) = size in pixels.
 */
void hd63484_fill_rect(int16_t x, int16_t y,
                       int16_t w, int16_t h,
                       uint8_t color)
{
    hd63484_set_color0(0x0);
    hd63484_set_color1(color);
    hd63484_set_solid_pattern();
    hd63484_amove(x, y + h - 1);                                      /* screen bottom-left of rect */
    hd63484_afrct(x + w - 1, y, AREA_NONE, COL_REG_IND, OPM_REPLACE); /* top-right */
}

/*
 * hd63484_draw_line – draw a line between two screen-space points.
 */
void hd63484_draw_line(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       uint8_t color)
{
    hd63484_set_color0(0x0);
    hd63484_set_color1(color);
    hd63484_set_solid_pattern();
    hd63484_amove(x0, y0);
    hd63484_aline(x1, y1, AREA_NONE, COL_REG_IND, OPM_REPLACE);
}

/*
 * hd63484_draw_char – draw one 8×8 character at screen position (sx, sy).
 *
 * The glyph is loaded into pattern RAM and stamped with PTN.
 * Each font byte is one row, MSB = leftmost pixel.
 * fg = foreground colour index (drawn on 1-bits).
 * bg = background colour index (drawn on 0-bits); use the same colour as
 *      the screen background for transparent-style text.
 *
 * PTN draws in chip-Y-up order: the stamp starts at the BOTTOM of the
 * cell (chip y = sy(sy+7)) and fills upward.  We reverse the row order
 * when loading the pattern RAM so the glyph appears upright on screen.
 */
void hd63484_draw_char(int16_t sx, int16_t sy, char c,
                       uint8_t fg, uint8_t bg)
{
    static int16_t last_c  = -1;
    static uint8_t last_fg = 0xFF, last_bg = 0xFF;

    if ((uint8_t)c >= 128) c = '?';

    if ((int16_t)(uint8_t)c != last_c) {
        hd63484_wptn(0, 8, font8x8[(uint8_t)c]);
        last_c = (int16_t)(uint8_t)c;
    }
    if (fg != last_fg) { hd63484_set_color1(fg); last_fg = fg; }
    if (bg != last_bg) { hd63484_set_color0(bg); last_bg = bg; }

    hd63484_amove(sx, sy + 7);
    hd63484_ptn(7, 7, 0, 0, AREA_NONE, COL_REG_IND, OPM_REPLACE);
}

/*
 * hd63484_draw_string – draw a NUL-terminated string left to right.
 * Advances 8 pixels per character.  Stops at end of string or if the
 * next character would start beyond the right edge of the screen.
 */
void hd63484_draw_string(int16_t sx, int16_t sy, const char *str,
                         uint8_t fg, uint8_t bg)
{
    while (*str)
    {
        hd63484_draw_char(sx, sy, *str, fg, bg);
        sx += 8;
        str++;
    }
}

uint8_t stringfg = PAL_WHITE;
uint8_t stringbg = PAL_GREEN;
uint16_t stringx = 8;
uint16_t stringy = 8;

void print_string(const char *str)
{
    while (*str) {
        bool advance_y = false;
        if (*str == '\n') {
            stringx = 8; 
            advance_y = true;
        } else if (*str == '\r') {
            stringx = 8;
        } else {
            hd63484_draw_char(stringx, stringy, *str, stringfg, stringbg);
            if ((stringx += 8) >= HTOTAL-8) { stringx = 8; advance_y = true; }
        }

        if (advance_y) {
            advance_y = false;
            stringy = stringy + 8;
            if (stringy >= VTOTAL-8) stringy = 8;
        }

        str++;
    }
}

void println(const char *str)
{
    print_string(str);
    print_string("\n");
}

void ramdac_reset(void)
{
    volatile uint8_t i;
    for (i = 0; i < 4; i++)
        ;
    for (i = 0; i < 4; i++)
        ;
    for (i = 0; i < 4; i++)
        ;

    *ramdac_index = 0;
    for (i = 0; i < 4; i++)
        ;
    *ramdac_mask = 0xff;

    uint16_t j;
    for (j = 0; j < 16; j++)
    {
        uint8_t r = (0x21 * ((j >> 0) & 1) + 0x47 * ((j >> 3) & 1) + 0x97 * ((j >> 0) & 1)) * 63 / 255;
        uint8_t g = (0x21 * ((j >> 1) & 1) + 0x47 * ((j >> 3) & 1) + 0x97 * ((j >> 1) & 1)) * 63 / 255;
        uint8_t b = (0x21 * ((j >> 2) & 1) + 0x47 * ((j >> 3) & 1) + 0x97 * ((j >> 2) & 1)) * 63 / 255;
        *ramdac_index = (uint8_t)j;
        *ramdac_palette = r;
        *ramdac_palette = g;
        *ramdac_palette = b;
    }
}
