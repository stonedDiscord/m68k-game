/*
 * gpu.h - HD63484 ACRTC Driver Library
 *
 * Hitachi HD63484 Advanced CRT Controller
 * For M68K homebrew systems
 *
 * Register map, command opcodes, and API based on the
 * 1985 HD63484 ACRTC User's Manual.
 *
 * Hardware assumptions:
 *   Address register / status register : 0x800080 (RS=0)
 *   Data / control register (FIFO entry): 0x800082 (RS=1)
 *
 * In 16-bit bus mode (which this driver targets):
 *   - Write to RS=0 selects the internal register via Address Register
 *   - Read  from RS=0 returns the Status Register
 *   - Read/write to RS=1 accesses the register chosen by AR,
 *     or the read/write FIFO when AR=0x00
 */

#ifndef GPU_H
#define GPU_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Hardware-mapped registers (your platform's wiring)
 * ---------------------------------------------------------------------------*/
extern volatile uint16_t *hd63484_addr;     /* RS=0 : write=AR, read=SR */
extern volatile uint16_t *hd63484_control;  /* RS=1 : data / FIFO        */

/* ---------------------------------------------------------------------------
 * Status Register (SR) bit masks  – read from RS=0
 * ---------------------------------------------------------------------------*/
#define SR_WFE  (1u << 0)   /* Write FIFO Empty              */
#define SR_WFR  (1u << 1)   /* Write FIFO Ready (not full)   */
#define SR_RFR  (1u << 2)   /* Read FIFO Ready               */
#define SR_RFF  (1u << 3)   /* Read FIFO Full                */
#define SR_LPD  (1u << 4)   /* Light Pen Detect              */
#define SR_CED  (1u << 5)   /* Command End                   */
#define SR_ARD  (1u << 6)   /* Area Detect                   */
#define SR_CER  (1u << 7)   /* Command Error                 */

/* ---------------------------------------------------------------------------
 * Directly-Accessible Register Addresses  (written to AR)
 *
 * Addresses rOO–r7F do NOT auto-increment AR.
 * Addresses r80–rFF DO auto-increment AR (allows block init).
 * ---------------------------------------------------------------------------*/

/* Hardware / FIFO entry */
#define REG_FE      0x00u   /* FIFO Entry (read/write FIFO)  */

/* Control registers (rO2–rO7) */
#define REG_CCR     0x02u   /* Command Control Register      */
#define REG_OMR     0x04u   /* Operation Mode Register       */
#define REG_DCR     0x06u   /* Display Control Register      */

/* Timing Control RAM  (r80–r9F, auto-increment)  */
#define REG_RCR     0x80u   /* Raster Count Register (RO)    */
#define REG_HSR     0x82u   /* Horizontal Sync               */
#define REG_HDR     0x84u   /* Horizontal Display            */
#define REG_VSR     0x86u   /* Vertical Sync                 */
#define REG_VDR     0x88u   /* Vertical Display              */
#define REG_SSW     0x8Au   /* Split Screen Width            */
#define REG_BCR     0x90u   /* Blink Control                 */
#define REG_HWR     0x92u   /* Horizontal Window Display     */
#define REG_VWR     0x94u   /* Vertical Window Display       */
#define REG_GCR     0x96u   /* Graphic Cursor                */

/* Display Control RAM (rC0–rDF, auto-increment)
 * RAR = Raster Address (first/last raster for char screens, 0 for graphic)
 * MWR = Memory Width in 16-bit words per line (CHR bit 15 = char/graphic select)
 * SAR = 20-bit Start Address split across H (bits 19-16) and L (bits 15-0)
 */
#define SCREEN_UPPER  0u // RAR0=rC0, MWR0=rC2, SAR0H=rC4, SAR0L=rC6
#define SCREEN_BASE   1u // RAR1=rC8, MWR1=rCA, SAR1H=rCC, SAR1L=rCE
#define SCREEN_LOWER  2u // RAR2=rD0, MWR2=rD2, SAR2H=rD4, SAR2L=rD6
#define SCREEN_WINDOW 3u // RAR3=rD8, MWR3=rDA, SAR3H=rDC, SAR3L=rDE

#define REG_RAR0    0xC0u   /* Screen 0 (Upper) Raster Address      */
#define REG_MWR0    0xC2u   /* Screen 0 Memory Width                 */
#define REG_SAR0H   0xC4u   /* Screen 0 Start Address High           */
#define REG_SAR0L   0xC6u   /* Screen 0 Start Address Low            */
#define REG_RAR1    0xC8u   /* Screen 1 (Base) Raster Address        */
#define REG_MWR1    0xCAu   /* Screen 1 Memory Width                 */
#define REG_SAR1H   0xCCu   /* Screen 1 Start Address High           */
#define REG_SAR1L   0xCEu   /* Screen 1 Start Address Low            */
#define REG_RAR2    0xD0u   /* Screen 2 (Lower) Raster Address       */
#define REG_MWR2    0xD2u   /* Screen 2 Memory Width                 */
#define REG_SAR2H   0xD4u   /* Screen 2 Start Address High           */
#define REG_SAR2L   0xD6u   /* Screen 2 Start Address Low            */
#define REG_RAR3    0xD8u   /* Screen 3 (Window) Raster Address      */
#define REG_MWR3    0xDAu   /* Screen 3 Memory Width                 */
#define REG_SAR3H   0xDCu   /* Screen 3 Start Address High           */
#define REG_SAR3L   0xDEu   /* Screen 3 Start Address Low            */
#define REG_CDR     0xE0u   /* Cursor Definition             */
#define REG_ZFR     0xEAu   /* Zoom Factor                   */
#define REG_LPAR    0xECu   /* Light Pen Address             */

/* ---------------------------------------------------------------------------
 * CCR – Command Control Register (r02–r03) bit masks
 * ---------------------------------------------------------------------------*/
/* High byte (r02) */
#define CCR_ABT     (1u << 15)  /* Abort – clears FIFOs, resets drawing  */
#define CCR_PSE     (1u << 14)  /* Pause command execution               */
#define CCR_DDM     (1u << 13)  /* Data DMA Mode enable                  */
#define CCR_CDM     (1u << 12)  /* Command DMA Mode enable               */
#define CCR_DRC     (1u << 11)  /* DMA Request Control: 0=burst,1=cycle  */
/* Graphic Bit Mode (bits 10–8) – use CCR_GBM_* helpers below            */
#define CCR_GBM_SHIFT 8u
#define CCR_GBM_MASK  (0x7u << CCR_GBM_SHIFT)
/* Low byte (r03) – interrupt enables */
#define CCR_CRE     (1u << 7)   /* Command Error interrupt enable        */
#define CCR_ARE     (1u << 6)   /* Area Detect interrupt enable          */
#define CCR_CEE     (1u << 5)   /* Command End interrupt enable          */
#define CCR_LPE     (1u << 4)   /* Light Pen interrupt enable            */
#define CCR_RFE     (1u << 3)   /* Read FIFO Full interrupt enable       */
#define CCR_RRE     (1u << 2)   /* Read FIFO Ready interrupt enable      */
#define CCR_WRE     (1u << 1)   /* Write FIFO Ready interrupt enable     */
#define CCR_WEE     (1u << 0)   /* Write FIFO Empty interrupt enable     */

/* Graphic Bit Mode values (pass to hd63484_set_gbm) */
#define GBM_1BPP    0x0u    /*  1 bit/pixel  – monochrome (16 px/word) */
#define GBM_2BPP    0x1u    /*  2 bits/pixel – 4 colours   (8 px/word) */
#define GBM_4BPP    0x2u    /*  4 bits/pixel – 16 colours  (4 px/word) */
#define GBM_8BPP    0x3u    /*  8 bits/pixel – 256 colours (2 px/word) */
#define GBM_16BPP   0x4u    /* 16 bits/pixel – 64K colours (1 px/word) */

/* ---------------------------------------------------------------------------
 * OMR – Operation Mode Register (r04–r05) bit masks
 * ---------------------------------------------------------------------------*/
#define OMR_MIS     (1u << 15)  /* Master(1) / Slave(0)                  */
#define OMR_STR     (1u << 14)  /* Start(1) / Stop(0) – set LAST         */
#define OMR_ACP     (1u << 13)  /* 0=display priority, 1=drawing priority*/
#define OMR_WSS     (1u << 12)  /* Window Smooth Scroll                  */
/* Cursor Display Skew bits 11–10 */
#define OMR_CSK_SHIFT 10u
#define OMR_CSK_MASK  (0x3u << OMR_CSK_SHIFT)
/* DISP Skew bits 9–8 */
#define OMR_DSK_SHIFT 8u
#define OMR_DSK_MASK  (0x3u << OMR_DSK_SHIFT)
#define OMR_RAM     (1u << 7)   /* 0=DRAM refresh mode, 1=SRAM mode      */
/* Graphic Address Increment bits 6–4 */
#define OMR_GAI_SHIFT 4u
#define OMR_GAI_MASK  (0x7u << OMR_GAI_SHIFT)
#define GAI_INC1    0x0u    /* increment by 1 every display cycle  */
#define GAI_INC2    0x1u    /* increment by 2                      */
#define GAI_INC4    0x2u    /* increment by 4                      */
#define GAI_INC8    0x3u    /* increment by 8                      */
#define GAI_CONST   0x4u    /* no increment                        */
#define GAI_INC1_2  0x5u    /* increment by 1 every 2 cycles       */
/* Access Mode bits 3–2 */
#define OMR_ACM_SHIFT 2u
#define OMR_ACM_MASK  (0x3u << OMR_ACM_SHIFT)
#define ACM_SINGLE      0x0u  /* Single access                      */
#define ACM_INTERLEAVED 0x2u  /* Interleaved (dual 0)               */
#define ACM_SUPER       0x3u  /* Superimposed (dual 1)              */
/* Raster Scan Mode bits 1–0 */
#define OMR_RSM_MASK  0x3u
#define RSM_NONINTERLACE 0x0u
#define RSM_INTERLACE_SYNC 0x2u
#define RSM_INTERLACE_SV   0x3u

/* ---------------------------------------------------------------------------
 * DCR – Display Control Register (r06–r07) bit masks
 * ---------------------------------------------------------------------------*/
#define DCR_DSP     (1u << 15)  /* DISP signal mode                      */
#define DCR_SE1     (1u << 14)  /* Split Enable 1 – Base screen on/off   */
#define DCR_SE0_SHIFT 12u       /* Split Enable 0 – Upper (bits 13–12)   */
#define DCR_SE0_MASK  (0x3u << DCR_SE0_SHIFT)
#define DCR_SE2_SHIFT 10u       /* Split Enable 2 – Lower (bits 11–10)   */
#define DCR_SE2_MASK  (0x3u << DCR_SE2_SHIFT)
#define DCR_SE3_SHIFT 8u        /* Split Enable 3 – Window (bits 9–8)    */
#define DCR_SE3_MASK  (0x3u << DCR_SE3_SHIFT)
/* Attribute bits 7–0 */
#define DCR_ATR_MASK  0xFFu

/* SE0/SE2/SE3 field values */
#define SE_DISABLED  0x0u   /* screen disabled (collapsed)     */
#define SE_BLANKED   0x2u   /* screen area kept, display off   */
#define SE_ENABLED   0x3u   /* screen active                   */

/* ---------------------------------------------------------------------------
 * Drawing Parameter Register addresses  (accessed via WPR/RPR commands)
 *
 * These are 16-bit "parameter register" numbers passed as the RN field
 * in the WPR/RPR opcode or as a parameter word.
 * ---------------------------------------------------------------------------*/
#define PR_CL0      0x00u   /* Color 0 Register                          */
#define PR_CL1      0x01u   /* Color 1 Register                          */
#define PR_CCMP     0x02u   /* Color Comparison Register                 */
#define PR_EDG      0x03u   /* Edge Color Register                       */
#define PR_MASK     0x04u   /* Mask Register                             */
#define PR_PRC0     0x05u   /* Pattern RAM Control (Pr05 – PPX/PPY/PZCX/PZCY) */
#define PR_PRC1     0x06u   /* Pattern RAM Control (Pr06 – PSX/PSY)      */
#define PR_PRC2     0x07u   /* Pattern RAM Control (Pr07 – PEX/PEY/PZX/PZY) */
#define PR_ADR_XMIN 0x08u   /* Area Definition XMIN                      */
#define PR_ADR_YMIN 0x09u   /* Area Definition YMIN                      */
#define PR_ADR_XMAX 0x0Au   /* Area Definition XMAX                      */
#define PR_ADR_YMAX 0x0Bu   /* Area Definition YMAX                      */
#define PR_RWP_H    0x0Cu   /* Read/Write Pointer High (DN + RWPH)       */
#define PR_RWP_L    0x0Du   /* Read/Write Pointer Low                    */
#define PR_DP_H     0x10u   /* Drawing Pointer High                      */
#define PR_DP_L     0x11u   /* Drawing Pointer Low + dot addr            */
#define PR_CP_X     0x12u   /* Current Pointer X                         */
#define PR_CP_Y     0x13u   /* Current Pointer Y                         */

/* Display Number field for RWP/DP (bits 15–14 of the High word) */
#define DN_UPPER    (0x0u << 14)
#define DN_BASE     (0x1u << 14)
#define DN_LOWER    (0x2u << 14)
#define DN_WINDOW   (0x3u << 14)

/* ---------------------------------------------------------------------------
 * Command opcodes
 *
 * All commands are 16-bit words written to the write FIFO (AR=0x00).
 * Graphic drawing commands embed AREA, COL, and OPM sub-fields.
 * ---------------------------------------------------------------------------*/

/* Register Access Commands
 *
 * Opcode bit layout from the manual command table:
 *   ORG  = 0000 0100 0000 0000 = 0x0400
 *   WPR  = 0000 1000 0000 RN  = 0x0800 | RN   (RN = parameter reg number, bits 7-0)
 *   RPR  = 0000 1100 0000 RN  = 0x0C00 | RN
 *   WPTN = 0001 1000 PRA[4:0] n[3:0] = 0x1800 | (pra<<4) | n
 *   RPTN = 0001 1100 PRA[4:0] n[3:0] = 0x1C00 | (pra<<4) | n
 *
 * Note: CMD_RPR must NOT equal CMD_ORG. ORG=0x0400, RPR starts at 0x0C00.
 */
#define CMD_ORG     0x0400u  /* Set Origin Point                */
#define CMD_WPR     0x0800u  /* Write Parameter Register        */
#define CMD_RPR     0x0C00u  /* Read Parameter Register         */
#define CMD_WPTN    0x1800u  /* Write Pattern RAM               */
#define CMD_RPTN    0x1C00u  /* Read Pattern RAM                */

/* Data Transfer Commands
 * Bit pattern for RD/WT/MOD: 1010 0CCC 0000 00MM
 *   RD   = 0xA100 (CCC=001)
 *   WT   = 0xA200 (CCC=010)
 *   MOD  = 0xA700 (CCC=111) | MM
 * Bit pattern for CLR/SCLR:  1010 11CC ...
 *   CLR  = 0xAC00
 *   SCLR = 0xAD00 | MM
 * CPY/SCPY: 1011 0CSS OSO ...
 *   CPY  = 0xB400
 *   SCPY = 0xBC00 | MM
 */
#define CMD_DRD     0x2400u  /* DMA Read  – base (|= address fields) */
#define CMD_DWT     0x2800u  /* DMA Write – base                     */
#define CMD_DMOD    0x2C00u  /* DMA Modify – base (| MM bits)        */
#define CMD_RD      0x4400u  /* Read one word                        */
#define CMD_WT      0x4800u  /* Write one word                       */
#define CMD_MOD     0x4C00u  /* Modify one word (| MM)               */
#define CMD_CLR     0x5800u  /* Clear area                           */
#define CMD_SCLR    0x5C00u  /* Selective Clear (| MM)               */
#define CMD_CPY     0x6000u  /* Copy area                            */
#define CMD_SCPY    0x7000u  /* Selective Copy (| MM)                */

/* Graphic Drawing Commands – lower byte = AREA|COL|OPM sub-fields */
#define CMD_AMOVE   0x8000u  /* Absolute Move                   */
#define CMD_RMOVE   0x8400u  /* Relative Move                   */
#define CMD_ALINE   0x8800u  /* Absolute Line   (| draw flags)  */
#define CMD_RLINE   0x8C00u  /* Relative Line                   */
#define CMD_ARCT    0x9000u  /* Absolute Rectangle              */
#define CMD_RRCT    0x9400u  /* Relative Rectangle              */
#define CMD_APLL    0x9800u  /* Absolute Polyline               */
#define CMD_RPLL    0x9C00u  /* Relative Polyline               */
#define CMD_APLG    0xA000u  /* Absolute Polygon                */
#define CMD_RPLG    0xA400u  /* Relative Polygon                */
#define CMD_CRCL    0xA800u  /* Circle                          */
#define CMD_ELPS    0xAC00u  /* Ellipse                         */
#define CMD_AARC    0xB000u  /* Absolute Arc                    */
#define CMD_RARC    0xB400u  /* Relative Arc                    */
#define CMD_AEARC   0xB800u  /* Absolute Ellipse Arc            */
#define CMD_REARC   0xBC00u  /* Relative Ellipse Arc            */
#define CMD_AFRCT   0xC000u  /* Absolute Filled Rectangle       */
#define CMD_RFRCT   0xC400u  /* Relative Filled Rectangle       */
#define CMD_PAINT   0xC800u  /* Paint (flood fill)              */
#define CMD_DOT     0xCC00u  /* Dot                             */
#define CMD_PTN     0xD000u  /* Pattern                         */
#define CMD_AGCPY   0xE000u  /* Absolute Graphic Copy           */
#define CMD_RGCPY   0xF000u  /* Relative Graphic Copy           */

/*
 * Sub-fields ORed into graphic drawing command opcodes (bits 7–0)
 *
 * AREA mode (bits 7–5)
 */
#define AREA_NONE   (0x0u << 5)  /* No area checking                */
#define AREA_CLIP   (0x1u << 5)  /* Clip to defined area            */
#define AREA_HIT_IN (0x2u << 5)  /* Hit detect – inside area        */
#define AREA_HIT_OUT (0x3u << 5) /* Hit detect – outside area       */

/* COL – Color Mode (bits 4–3): how Pattern RAM is interpreted */
#define COL_REG_IND (0x0u << 3)  /* Pattern RAM selects CL0/CL1     */
#define COL_DIRECT  (0x2u << 3)  /* Pattern RAM data written directly*/

/* OPM – Operation Mode (bits 2–0): pixel logical operation */
#define OPM_REPLACE (0x0u)  /* Replace fb with color data          */
#define OPM_OR      (0x1u)  /* OR                                  */
#define OPM_AND     (0x2u)  /* AND                                 */
#define OPM_EOR     (0x3u)  /* EOR (XOR)                           */
#define OPM_CEQC    (0x4u)  /* Conditional replace if P == CCMP    */
#define OPM_CNEC    (0x5u)  /* Conditional replace if P != CCMP    */
#define OPM_CLTC    (0x6u)  /* Conditional replace if P <  CL      */
#define OPM_CGTC    (0x7u)  /* Conditional replace if P >  CL      */

/* Modify Mode bits for DMA/modify commands (bits 1–0) */
#define MM_REPLACE  0x0u
#define MM_OR       0x1u
#define MM_AND      0x2u
#define MM_EOR      0x3u

/* CRCL clockwise flag (bit 8) */
#define CRCL_CW     (1u << 8)

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
#define VRAM_LOWER   0x00000UL
#define VRAM_UPPER   0x80000UL

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/* Low-level bus helpers */
void     hd63484_write_ar(uint8_t reg_addr);
uint16_t hd63484_read_sr(void);
void     hd63484_write_reg(uint8_t reg_addr, uint16_t value);
uint16_t hd63484_read_reg(uint8_t reg_addr);

/* FIFO helpers */
void     hd63484_wait_wfr(void);    /* wait until write FIFO ready */
void     hd63484_wait_wfe(void);    /* wait until write FIFO empty */
void     hd63484_wait_rfr(void);    /* wait until read FIFO ready  */
void     hd63484_wait_ced(void);    /* wait until command end (chip idle) */
void     hd63484_fifo_write(uint16_t word);
uint16_t hd63484_fifo_read(void);

/* Chip-level control */
void     hd63484_reset(void);
void     hd63484_abort(void);
void     hd63484_start(void);
void     hd63484_stop(void);

/* Full initialization with hardcoded values */
void     hd63484_init(void);

/* Drawing parameter registers (via WPR/RPR over FIFO) */
void     hd63484_wpr(uint8_t pr_addr, uint16_t value);
uint16_t hd63484_rpr(uint8_t pr_addr);

/* Pattern RAM (via WPTN/RPTN over FIFO) */
void     hd63484_wptn(uint8_t pra, uint8_t n, const uint16_t *data);
void     hd63484_rptn(uint8_t pra, uint8_t n, uint16_t *data);

/* Origin – set by hd63484_init(), not normally called directly */
void     hd63484_set_origin(uint8_t dn, uint32_t addr, uint8_t dot);

void     hd63484_disable_screen(uint8_t screen);
void     hd63484_enable_screen(uint8_t screen, uint32_t vram_addr, uint16_t mem_width);

/* ---------------------------------------------------------------------------
 * Coordinate system
 *
 * All drawing API functions in this library accept Y in screen-space
 * (y=0 = TOP of screen, y increases downward) and flip internally.
 * Use hd63484_sy() if you call chip functions directly.
 * ---------------------------------------------------------------------------*/
extern int16_t hd63484_screen_height; /* initialised by hd63484_init() */

static inline int16_t hd63484_sy(int16_t screen_y)
{
    return hd63484_screen_height - 1 - screen_y;
}

/* ---------------------------------------------------------------------------
 * hd63484_color_reg – build a CL0 / CL1 / EDG register value from a 4bpp index.
 *
 * MAME extracts the per-pixel color as:
 *   cl = (m_cl >> ((x % ppw) * bpp)) & mask
 * For 4bpp: ppw=4, bpp=4 — so the same 4-bit index must sit in all four nibbles.
 * Passing a plain index (e.g. 1 for red) only fills bits 3:0; every other column
 * draws color 0 (black), producing vertical stripes.
 * ---------------------------------------------------------------------------*/
static inline uint16_t hd63484_color_reg(uint8_t index)
{
    uint16_t n = (uint16_t)(index & 0x0Fu);
    return n | (n << 4) | (n << 8) | (n << 12);
}

/* Colour setup – pass a 4-bit palette index; nibble replication is automatic */
void     hd63484_set_color0(uint8_t color);
void     hd63484_set_color1(uint8_t color);
void     hd63484_set_edge_color(uint8_t color);
void     hd63484_set_mask(uint16_t mask);          /* raw bitmask, not a color */
void     hd63484_set_compare_color(uint8_t color);

/* Area definition */
void     hd63484_set_area(int16_t xmin, int16_t ymin,
                           int16_t xmax, int16_t ymax);

/* Drawing pointer (physical frame buffer address) */
void     hd63484_set_rwp(uint8_t dn, uint32_t addr);
void     hd63484_set_dp (uint8_t dn, uint32_t addr, uint8_t dot);

/* Pattern RAM control – simple solid pattern helper */
void     hd63484_set_solid_pattern(void);

/* ---- Graphic drawing commands ---- */

/* Move current pointer */
void     hd63484_amove(int16_t x, int16_t y);
void     hd63484_rmove(int16_t dx, int16_t dy);

/* Lines */
void     hd63484_aline(int16_t x, int16_t y,
                        uint8_t area, uint8_t col, uint8_t opm);
void     hd63484_rline(int16_t dx, int16_t dy,
                        uint8_t area, uint8_t col, uint8_t opm);

/* Rectangles (outline) */
void     hd63484_arct(int16_t x, int16_t y,
                       uint8_t area, uint8_t col, uint8_t opm);
void     hd63484_rrct(int16_t dx, int16_t dy,
                       uint8_t area, uint8_t col, uint8_t opm);

/* Filled rectangles */
void     hd63484_afrct(int16_t x, int16_t y,
                        uint8_t area, uint8_t col, uint8_t opm);
void     hd63484_rfrct(int16_t dx, int16_t dy,
                        uint8_t area, uint8_t col, uint8_t opm);

/* Circle */
void     hd63484_crcl(uint16_t radius, int clockwise,
                       uint8_t area, uint8_t col, uint8_t opm);

/* Ellipse */
void     hd63484_elps(uint16_t a, uint16_t b,
                       int16_t dx, int16_t dy,
                       int clockwise,
                       uint8_t area, uint8_t col, uint8_t opm);

/* Dot */
void     hd63484_dot(uint8_t area, uint8_t col, uint8_t opm);

/* Paint (flood fill) */
void     hd63484_paint(int stop_at_edge,
                        uint8_t area, uint8_t col, uint8_t opm);

/* Pattern blit */
void     hd63484_ptn(int16_t sz_x, int16_t sz_y,
                      uint8_t sl, uint8_t so,
                      uint8_t area, uint8_t col, uint8_t opm);

/* ---- Data transfer commands ---- */

/* Single word read/write/modify */
uint16_t hd63484_rd(void);
void     hd63484_wt(uint16_t data);
void     hd63484_mod(uint16_t data, uint8_t mm);

/* Block clear (RWP-based; fill=data value, ax/ay=dimensions in words) */
void     hd63484_clr(uint16_t fill, int16_t ax, int16_t ay);

/* Block copy */
void     hd63484_cpy(uint16_t sah, uint16_t sal,
                      int16_t ax, int16_t ay);

/* Polyline helpers (n points, absolute) */
void     hd63484_apll(uint8_t n, const int16_t *xy,
                       uint8_t area, uint8_t col, uint8_t opm);

/* Polygon helpers (n points, absolute) */
void     hd63484_aplg(uint8_t n, const int16_t *xy,
                       uint8_t area, uint8_t col, uint8_t opm);

/* High-level convenience */
void     hd63484_clear_screen(uint8_t color,
                               int16_t w, int16_t h);
void     hd63484_fill_rect(int16_t x, int16_t y,
                            int16_t w, int16_t h,
                            uint8_t color);
void     hd63484_draw_line(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1,
                            uint8_t color);

/* ---------------------------------------------------------------------------
 * Text rendering  (8x8 bitmap font via Pattern RAM PTN command)
 *
 * Font data must be a uint8_t [128][8] table where each byte is one
 * row of the glyph, MSB = leftmost pixel.  Pass a pointer to your
 * font table once with hd63484_set_font(); subsequent draw_char /
 * draw_string calls use it automatically.
 *
 * Coordinates are screen-space (y=0 = top row, y increases downward).
 * Each character cell is 8×8 pixels.  Characters are clipped if they
 * would extend beyond (SCREEN_W-1, SCREEN_H-1).
 * ---------------------------------------------------------------------------*/

extern uint8_t stringfg;
extern uint8_t stringbg;
extern uint16_t stringx;
extern uint16_t stringy;

void hd63484_set_font(const uint8_t (*font)[8]);
void hd63484_draw_char  (int16_t sx, int16_t sy, char c);
void hd63484_draw_string(int16_t sx, int16_t sy, const char *str);

void print_string(const char *str);
void println(const char *str);

void ramdac_reset(void);

#endif /* GPU_H */
