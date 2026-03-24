
/* rtc.c – RTC-62421 reader, returns timespec with manually calculated epoch seconds */
 
#include "rtc.h"
 
/* BCD nibble pair → integer */
static inline int bcd(uint8_t tens, uint8_t units)
{
    return (int)tens * 10 + (int)units;
}

int rtc_get_timespec(struct tm *t)
{
    if (!t) return -1;

    uint8_t h10, h1, mi10, mi1, s10, s1;
    uint8_t y10, y1, mo10, mo1, d10, d1, w;
 
    RTC_HOLD_SET();
    while (RTC_IS_BUSY())
        ;
 
    s10  = RTC_READ(RTC_REG_S10);
    s1   = RTC_READ(RTC_REG_S1);
    mi10 = RTC_READ(RTC_REG_MI10);
    mi1  = RTC_READ(RTC_REG_MI1);
    h10  = RTC_READ(RTC_REG_H10) & 0x3;   /* strip PM/AM flag */
    h1   = RTC_READ(RTC_REG_H1);
    d10  = RTC_READ(RTC_REG_D10);
    d1   = RTC_READ(RTC_REG_D1);
    mo10 = RTC_READ(RTC_REG_MO10);
    mo1  = RTC_READ(RTC_REG_MO1);
    y10  = RTC_READ(RTC_REG_Y10);
    y1   = RTC_READ(RTC_REG_Y1);
    w    = RTC_READ(RTC_REG_W);
    (void)w;  /* day of week not currently used */
 
    RTC_HOLD_CLR();

    if (d1 == 0 || mo1 == 0) /* day and month can't be zero */
    {
        return -1; /* invalid BCD value(s) */
    }

    t->tm_sec  = bcd(s10,  s1);
    t->tm_min  = bcd(mi10, mi1);
    t->tm_hour = bcd(h10,  h1);
    t->tm_mday = bcd(d10,  d1);
    t->tm_mon  = bcd(mo10, mo1) - 1;
    t->tm_year = 2000 + bcd(y10, y1) - 1900;
    t->tm_wday = 0;
    t->tm_yday = 0;
    t->tm_isdst = -1;

    return 0;
}
 