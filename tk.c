#include "tk.h"

/* --------------------------------------------------------------------------- tk_read() – matches observed firmware behaviour:   - no R-bit halt before reading (firmware doesn't do it either)   - clears control register to 0 after reading (as seen in disassembly) --------------------------------------------------------------------------- */
int tk_read(struct tm *t)
{
    if (!t) return -1;

    uint8_t secs  = TK_SECS  & 0x7Fu;  /* strip ST              */
    uint8_t mins  = TK_MINS  & 0x7Fu;
    uint8_t hours = TK_HOURS & 0x3Fu;
    uint8_t day   = TK_DAY   & 0x07u;  /* strip FT, reserved    */
    uint8_t date  = TK_DATE  & 0x3Fu;
    uint8_t month = TK_MONTH & 0x1Fu;
    uint8_t year  = TK_YEAR;


    if (month == 0 || date == 0 || day == 0) /* month, date, and day can't be zero */
    {
        return -1; /* invalid BCD value(s) */
    }

    TK_CTRL = 0;                        /* drop R bit, as firmware does */

    t->tm_sec  = (int)TK_BCD_TO_BIN(secs);
    t->tm_min  = (int)TK_BCD_TO_BIN(mins);
    t->tm_hour = (int)TK_BCD_TO_BIN(hours);
    t->tm_wday = (int)day - 1;          /* chip: 1-7, tm_wday: 0-6      */
    t->tm_mday = (int)TK_BCD_TO_BIN(date);
    t->tm_mon  = (int)TK_BCD_TO_BIN(month) - 1;
    t->tm_year = (int)TK_BCD_TO_BIN(year) + 100;

    t->tm_yday  = 0;
    t->tm_isdst = -1;
    return 0;
}

/* --------------------------------------------------------------------------- tk_write() – uses W-bit handshake per datasheet section 3.2 --------------------------------------------------------------------------- */
int tk_write(const struct tm *t)
{
    if (!t) return -1;

    TK_CTRL = TK_CTRL_W;

    TK_SECS  = TK_BIN_TO_BCD((unsigned)t->tm_sec);
    TK_MINS  = TK_BIN_TO_BCD((unsigned)t->tm_min);
    TK_HOURS = TK_BIN_TO_BCD((unsigned)t->tm_hour);
    TK_DAY   = TK_BIN_TO_BCD((unsigned)(t->tm_wday + 1));
    TK_DATE  = TK_BIN_TO_BCD((unsigned)t->tm_mday);
    TK_MONTH = TK_BIN_TO_BCD((unsigned)(t->tm_mon + 1));
    TK_YEAR  = TK_BIN_TO_BCD((unsigned)(t->tm_year % 100));

    TK_CTRL = 0;    /* clearing W transfers shadow registers to live counters */

    return 0;
}
