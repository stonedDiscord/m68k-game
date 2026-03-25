#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * Epson RTC-62421 / RTC-62423  –  memory-mapped register header
 *
 * The chip exposes 16 x 4-bit registers on a multiplexed 4-bit bus.
 * In a typical 8-bit system the chip is wired so that each register
 * occupies one byte-address; only D0..D3 are significant.
 *
 * Adjust RTC_BASE to match your hardware address-decode.
 * ---------------------------------------------------------------------------
 */

#define RTC_BASE  0x400001UL

/* Base pointer – each register is one address-unit apart               */
static volatile uint8_t * const rtc_address = (volatile uint8_t *)RTC_BASE;

/* ---------------------------------------------------------------------------
 * Register addresses  (A3:A0  →  offset from RTC_BASE)
 * ---------------------------------------------------------------------------
 */
#define RTC_REG_S1      0x0   /* 1-second digit      (0–9)              */
#define RTC_REG_S10     0x1   /* 10-second digit     (0–5)              */
#define RTC_REG_MI1     0x2   /* 1-minute digit      (0–9)              */
#define RTC_REG_MI10    0x3   /* 10-minute digit     (0–5)              */
#define RTC_REG_H1      0x4   /* 1-hour digit        (0–9)              */
#define RTC_REG_H10     0x5   /* 10-hour digit       (0–2 / 0–1 in 12H) */
#define RTC_REG_D1      0x6   /* 1-day digit         (0–9)              */
#define RTC_REG_D10     0x7   /* 10-day digit        (0–3)              */
#define RTC_REG_MO1     0x8   /* 1-month digit       (0–9)              */
#define RTC_REG_MO10    0x9   /* 10-month digit      (0–1)              */
#define RTC_REG_Y1      0xA   /* 1-year digit        (0–9)              */
#define RTC_REG_Y10     0xB   /* 10-year digit       (0–9)              */
#define RTC_REG_W       0xC   /* Week register       (0–6)              */
#define RTC_REG_CD      0xD   /* Control register D                     */
#define RTC_REG_CE      0xE   /* Control register E                     */
#define RTC_REG_CF      0xF   /* Control register F                     */

/* ---------------------------------------------------------------------------
 * Control register D  (address 0xD)
 * ---------------------------------------------------------------------------
 */
#define RTC_CD_30S_ADJ      (1u << 3)   /* 30-second adjust (write 1 to trigger) */
#define RTC_CD_IRQ_FLAG     (1u << 2)   /* IRQ flag (write 0 to clear, else 1)   */
#define RTC_CD_BUSY         (1u << 1)   /* Busy flag – read only (valid when HOLD=1) */
#define RTC_CD_HOLD         (1u << 0)   /* Hold: freeze counters for safe read   */

/* ---------------------------------------------------------------------------
 * Control register E  (address 0xE)
 * ---------------------------------------------------------------------------
 */
#define RTC_CE_T1           (1u << 3)   /* Interrupt period select bit 1         */
#define RTC_CE_T0           (1u << 2)   /* Interrupt period select bit 0         */
#define RTC_CE_24_12        (1u << 1)   /* 1 = 24H mode, 0 = 12H mode           */
#define RTC_CE_ITRPT_STND   (1u << 0)   /* 1 = interrupt, 0 = standard           */

/* Interrupt period selection (T1:T0) */
#define RTC_CE_ITRPT_64HZ   (0u << 2)   /* 64 Hz                                 */
#define RTC_CE_ITRPT_1HZ    (1u << 2)   /* 1 Hz                                  */
#define RTC_CE_ITRPT_1MIN   (2u << 2)   /* 1-minute carry                        */
#define RTC_CE_ITRPT_1HR    (3u << 2)   /* 1-hour carry                          */

/* ---------------------------------------------------------------------------
 * Control register F  (address 0xF)
 * ---------------------------------------------------------------------------
 */
#define RTC_CF_TEST         (1u << 3)   /* Test mode – always write 0 in normal use */
#define RTC_CF_24_12        (1u << 2)   /* 24/12H (mirror of CE bit in some docs)   */
#define RTC_CF_STOP         (1u << 1)   /* 1 = stop oscillator                      */
#define RTC_CF_RESET        (1u << 0)   /* 1 = reset (must be set before TEST bit)  */

/* H10 register: PM/AM flag in 12-hour mode */
#define RTC_H10_PM          (1u << 2)   /* 1 = PM, 0 = AM  (12H mode only)      */

/* ---------------------------------------------------------------------------
 /* Register access macros
  * ---------------------------------------------------------------------------
  */
 #define RTC_READ(reg)           (rtc_address[(reg) * 2] & 0x0Fu)
 #define RTC_WRITE(reg, val)     (rtc_address[(reg) * 2] = (uint8_t)((val) & 0x0Fu))


/* ---------------------------------------------------------------------------
 * Convenience: read/write the full time & date under HOLD
 *
 * Always set HOLD before reading multiple registers so the internal
 * counters cannot ripple between reads.  Clear HOLD when done.
 * Check BUSY after asserting HOLD; if set, wait until it clears.
 * ---------------------------------------------------------------------------
 */
#define RTC_HOLD_SET()   RTC_WRITE(RTC_REG_CD, RTC_READ(RTC_REG_CD) |  RTC_CD_HOLD)
#define RTC_HOLD_CLR()   RTC_WRITE(RTC_REG_CD, RTC_READ(RTC_REG_CD) & ~RTC_CD_HOLD)
#define RTC_IS_BUSY()    (RTC_READ(RTC_REG_CD) & RTC_CD_BUSY)

/* Read RTC and return POSIX timespec */
int rtc_get(struct tm *t);
int rtc_set(const struct tm *t);

/* ---------------------------------------------------------------------------
 * Timing constraints (from datasheet, VDD=5V, Ta=-40..+85°C)
 *
 * tCIS  CS1 setup time            1000 ns min
 * tCIH  CS1 hold time             1000 ns min
 * tAS   Address setup time          25 ns min
 * tAH   Address hold time           25 ns min
 * tAW   ALE pulse width             40 ns min
 * tALW  ALE before WRITE            10 ns min
 * tALR  ALE before READ             10 ns min
 * tWAL  ALE after WRITE             20 ns min
 * tRAL  ALE after READ              10 ns min
 * tWW   WRITE pulse width          120 ns min
 * tRD   RD/ to data valid          120 ns max
 * tDR   Data hold after RD/          0 ns min
 * tDS   Data setup time             10 ns min
 * tDH   Data hold time              60 ns min
 * tRCV  RD//WR/ recovery time       45 ns max
 * ---------------------------------------------------------------------------
 */

#endif /* RTC_H */