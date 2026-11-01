#include <time.h>
#include <unistd.h>
#include <kernel/syscall.h>

time_t time(time_t* t) {
    return __syscall3(SYS_time, (int)t, 0, 0);
}

int clock_gettime(int clk, struct timespec* ts) {
    return __syscall3(SYS_clock_gettime, clk, (int)ts, 0);
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)rem;
    return __syscall3(SYS_nanosleep, (int)req, 0, 0);
}

unsigned sleep(unsigned sec) {
    struct timespec ts = { (int)sec, 0 };
    return nanosleep(&ts, 0) < 0 ? sec : 0;
}

int usleep(unsigned usec) {
    struct timespec ts = { (int)(usec / 1000000u), (int)(usec % 1000000u) * 1000 };
    return nanosleep(&ts, 0);
}

struct tm* gmtime_r(const time_t* t, struct tm* o) {
    unsigned s = (unsigned)*t;
    unsigned days = s / 86400, rem = s % 86400;
    o->tm_hour = (int)(rem / 3600);
    o->tm_min = (int)(rem % 3600 / 60);
    o->tm_sec = (int)(rem % 60);
    o->tm_wday = (int)((days + 4) % 7);                 /* 1970-01-01 was a thursday */

    // civil from days (howard hinnant)
    int z = (int)days + 719468;
    int era = z / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = (int)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;
    o->tm_year = y - 1900;
    o->tm_mon = (int)m - 1;
    o->tm_mday = (int)d;

    static const int cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    o->tm_yday = cum[m - 1] + (int)d - 1 + (leap && m > 2);
    return o;
}
