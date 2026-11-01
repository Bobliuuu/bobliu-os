#include <stdio.h>
#include <time.h>

int main(void) {
    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* mons[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    time_t t = time(0);
    struct tm tm;
    gmtime_r(&t, &tm);
    printf("%s %s %d %d:%d:%d UTC %d (epoch %d)\n", days[tm.tm_wday], mons[tm.tm_mon], tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900, (int)t);
    return 0;
}
