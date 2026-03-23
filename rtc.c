
/* rtc.c – RTC-62421 reader, returns timespec with manually calculated epoch seconds */
 
#include "rtc.h"
 
/* BCD nibble pair → integer */
static inline int bcd(uint8_t tens, uint8_t units)
{
    return (int)tens * 10 + (int)units;
}

/* Days per month (non-leap year) */
static const uint8_t days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* Is leap year? */
static int is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* Days since epoch (1970-01-01) */
static uint32_t days_since_epoch(int year, int month, int day)
{
    uint32_t days = 0;
    for (int y = 1970; y < year; y++)
    {
        days += is_leap_year(y) ? 366 : 365;
    }
    for (int m = 1; m < month; m++)
    {
        days += days_per_month[m - 1];
        if (m == 2 && is_leap_year(year))
            days++;
    }
    days += day - 1;
    return days;
}

struct timespec rtc_get_timespec(void)
{
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
 
    RTC_HOLD_CLR();
 
    int sec   = bcd(s10,  s1);
    int min   = bcd(mi10, mi1);
    int hour  = bcd(h10,  h1);
    int day   = bcd(d10,  d1);
    int month = bcd(mo10, mo1);
    int year  = 2000 + bcd(y10, y1);
 
    /* Calculate Unix timestamp manually */
    uint32_t days = days_since_epoch(year, month, day);
    uint32_t secs = days * 86400UL + hour * 3600UL + min * 60UL + sec;
 
    struct timespec ts;
    ts.tv_sec  = (time_t)secs;
    ts.tv_nsec = 0;
    return ts;
}
 