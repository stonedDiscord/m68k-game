#ifndef TK_H
#define TK_H
#include <stdint.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * ST M48T08 – memory-mapped register header
 * M68k system: 16-bit data bus, M48T08 on low byte only.
 * Registers are word-spaced (stride 2).
 *
 * Override TK_BASE before including this header if the chip is mapped
 * differently on another board revision, e.g.:
 *   #define TK_BASE 0xFF0000UL
 *   #include "tk.h"
 * ---------------------------------------------------------------------------
 */

#ifndef TK_BASE
#define TK_BASE 0xFFFFF0UL
#endif

/* ---------------------------------------------------------------------------
 * Register addresses derived from TK_BASE (word stride = 2)
 * ---------------------------------------------------------------------------
 */
#define TK_ADDR_CTRL    ((volatile uint8_t *)(TK_BASE + 0x00u))  /* 0x1FF8 */
#define TK_ADDR_SECS    ((volatile uint8_t *)(TK_BASE + 0x02u))  /* 0x1FF9 */
#define TK_ADDR_MINS    ((volatile uint8_t *)(TK_BASE + 0x04u))  /* 0x1FFA */
#define TK_ADDR_HOURS   ((volatile uint8_t *)(TK_BASE + 0x06u))  /* 0x1FFB */
#define TK_ADDR_DAY     ((volatile uint8_t *)(TK_BASE + 0x08u))  /* 0x1FFC */
#define TK_ADDR_DATE    ((volatile uint8_t *)(TK_BASE + 0x0Au))  /* 0x1FFD */
#define TK_ADDR_MONTH   ((volatile uint8_t *)(TK_BASE + 0x0Cu))  /* 0x1FFE */
#define TK_ADDR_YEAR    ((volatile uint8_t *)(TK_BASE + 0x0Eu))  /* 0x1FFF */

/* Dereferenced register accessors                                       */
#define TK_CTRL         (*TK_ADDR_CTRL)
#define TK_SECS         (*TK_ADDR_SECS)
#define TK_MINS         (*TK_ADDR_MINS)
#define TK_HOURS        (*TK_ADDR_HOURS)
#define TK_DAY          (*TK_ADDR_DAY)
#define TK_DATE         (*TK_ADDR_DATE)
#define TK_MONTH        (*TK_ADDR_MONTH)
#define TK_YEAR         (*TK_ADDR_YEAR)

/* ---------------------------------------------------------------------------
 * Control register bits  (TK_BASE + 0x00)
 * ---------------------------------------------------------------------------
 */
#define TK_CTRL_W           (1u << 7)   /* WRITE – halt updates, load counters */
#define TK_CTRL_R           (1u << 6)   /* READ  – halt updates, safe to read  */
#define TK_CTRL_S           (1u << 5)   /* Sign  – 1 = positive calibration    */
#define TK_CTRL_CAL_MASK    0x1Fu       /* 5-bit calibration field             */

/* ---------------------------------------------------------------------------
 * Seconds register bits  (TK_BASE + 0x02)
 * ---------------------------------------------------------------------------
 */
#define TK_SECS_ST          (1u << 7)   /* STOP – 1 = oscillator halted        */

/* ---------------------------------------------------------------------------
 * Day register bits  (TK_BASE + 0x08)
 * ---------------------------------------------------------------------------
 */
#define TK_DAY_FT           (1u << 6)   /* Frequency Test – 512 Hz on DQ0      */

/* ---------------------------------------------------------------------------
 * BCD helpers
 * ---------------------------------------------------------------------------
 */
#define TK_BCD_TO_BIN(b)    ( ((b) >> 4) * 10u + ((b) & 0x0Fu) )
#define TK_BIN_TO_BCD(n)    ( (((n) / 10u) << 4) | ((n) % 10u) )

int tk_get(struct tm *t);
int tk_set(const struct tm *t);

#endif /* TK_H */