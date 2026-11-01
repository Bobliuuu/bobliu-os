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
};

#define CLOCK_REALTIME  0            /* utc wall clock */
#define CLOCK_MONOTONIC 1            /* since boot */

#ifdef __cplusplus
extern "C" {
#endif

time_t time(time_t* t);
int clock_gettime(int clk, struct timespec* ts);
int nanosleep(const struct timespec* req, struct timespec* rem);   /* rem is not filled in */
struct tm* gmtime_r(const time_t* t, struct tm* out);

#ifdef __cplusplus
}
#endif

#endif
