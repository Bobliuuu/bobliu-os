#ifndef _TIME_H
#define _TIME_H 1

typedef int time_t;

struct timespec {
    int tv_sec;
    int tv_nsec;
};

struct tm {
    int tm_sec, tm_min, tm_hour;
    int tm_mday, tm_mon, tm_year;    /* year since 1900, month 0-11 */
    int tm_wday, tm_yday;
    int tm_isdst;                    /* always 0: the clock is utc */
};

typedef long clock_t;
#define CLOCKS_PER_SEC 1000

#include <stddef.h>
#define CLOCK_REALTIME  0            /* utc wall clock */
#define CLOCK_MONOTONIC 1            /* since boot */

#ifdef __cplusplus
extern "C" {
#endif

time_t time(time_t* t);
int clock_gettime(int clk, struct timespec* ts);
int nanosleep(const struct timespec* req, struct timespec* rem);   /* rem is not filled in */
struct tm* gmtime_r(const time_t* t, struct tm* out);
struct tm* gmtime(const time_t* t);
struct tm* localtime(const time_t* t);          /* utc: there are no time zones */
struct tm* localtime_r(const time_t* t, struct tm* out);
time_t mktime(struct tm* tm);                   /* normalises tm, utc */
size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm);   /* %a %A %b %B %c %d %H %I %j %m %M %p %S %U %w %x %X %y %Y %Z %% %e %F %T %s %n %t */
double difftime(time_t a, time_t b);
clock_t clock(void);                            /* ms since boot */
char* asctime(const struct tm* tm);
char* ctime(const time_t* t);

#ifdef __cplusplus
}
#endif

#endif
