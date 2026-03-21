
/* rtc.c – RTC-62421 reader, returns POSIX struct timespec via mktime */
 
#include <time.h>
#include <stdint.h>
#include "rtc.h"
 
/* BCD nibble pair → integer */
static inline int bcd(uint8_t tens, uint8_t units)
{
    return (int)tens * 10 + (int)units;
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
 
    struct tm t = {
        .tm_sec   = bcd(s10,  s1),
        .tm_min   = bcd(mi10, mi1),
        .tm_hour  = bcd(h10,  h1),
        .tm_mday  = bcd(d10,  d1),
        .tm_mon   = bcd(mo10, mo1) - 1,    /* tm_mon is 0-11, RTC is 1-12 */
        .tm_year  = 100 + bcd(y10, y1),    /* tm_year is years since 1900  */
        .tm_wday  = (int)w,                 /* RTC week: 0=Sun..6=Sat, same as tm */
        .tm_yday  = -1,                     /* let mktime compute this      */
        .tm_isdst = -1,                     /* let mktime determine DST     */
    };
 
    struct timespec ts;
    ts.tv_sec  = mktime(&t);
    ts.tv_nsec = 0;   /* RTC has 1-second resolution */
    return ts;
}
 