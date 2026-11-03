// calendar helpers on top of gmtime_r (the clock is utc, there are no time zones)
#include <time.h>
#include <stdio.h>
#include <string.h>

static struct tm g_tm;

struct tm* gmtime(const time_t* t) { return gmtime_r(t, &g_tm); }
struct tm* localtime(const time_t* t) { return gmtime_r(t, &g_tm); }
struct tm* localtime_r(const time_t* t, struct tm* o) { return gmtime_r(t, o); }
double difftime(time_t a, time_t b) { return (double)(a - b); }

clock_t clock(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (clock_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

time_t mktime(struct tm* tm) {
    // normalise month first, then count days since 1970
    int y = tm->tm_year + 1900 + tm->tm_mon / 12;
    int mon = tm->tm_mon % 12;
    if (mon < 0) { mon += 12; y--; }
    static const int cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    long days = 0;
    for (int i = 1970; i < y; i++) days += is_leap(i) ? 366 : 365;
    for (int i = 1969; i >= y; i--) days -= is_leap(i) ? 366 : 365;
    days += cum[mon] + (is_leap(y) && mon > 1) + tm->tm_mday - 1;
    long secs = days * 86400L + tm->tm_hour * 3600L + tm->tm_min * 60L + tm->tm_sec;
    time_t t = (time_t)secs;
    gmtime_r(&t, tm);
    return t;
}

static const char* wd[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char* mn[] = { "January", "February", "March", "April", "May", "June", "July", "August",
                            "September", "October", "November", "December" };

size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm) {
    size_t n = 0;
    char tmp[32];
#define EMIT_STR(str) do { const char* q_ = (str); while (*q_) { if (n + 1 >= max) return 0; s[n++] = *q_++; } } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { if (n + 1 >= max) return 0; s[n++] = *fmt; continue; }
        fmt++;
        switch (*fmt) {
        case 'a': snprintf(tmp, 4, "%s", wd[tm->tm_wday % 7]); EMIT_STR(tmp); break;
        case 'A': EMIT_STR(wd[tm->tm_wday % 7]); break;
        case 'b': case 'h': snprintf(tmp, 4, "%s", mn[tm->tm_mon % 12]); EMIT_STR(tmp); break;
        case 'B': EMIT_STR(mn[tm->tm_mon % 12]); break;
        case 'c': snprintf(tmp, sizeof tmp, "%.3s %.3s %2d", wd[tm->tm_wday % 7], mn[tm->tm_mon % 12], tm->tm_mday); EMIT_STR(tmp);
                  snprintf(tmp, sizeof tmp, " %02d:%02d:%02d %d", tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900); EMIT_STR(tmp); break;
        case 'd': snprintf(tmp, sizeof tmp, "%02d", tm->tm_mday); EMIT_STR(tmp); break;
        case 'e': snprintf(tmp, sizeof tmp, "%2d", tm->tm_mday); EMIT_STR(tmp); break;
        case 'F': snprintf(tmp, sizeof tmp, "%d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); EMIT_STR(tmp); break;
        case 'H': snprintf(tmp, sizeof tmp, "%02d", tm->tm_hour); EMIT_STR(tmp); break;
        case 'I': snprintf(tmp, sizeof tmp, "%02d", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12); EMIT_STR(tmp); break;
        case 'j': snprintf(tmp, sizeof tmp, "%03d", tm->tm_yday + 1); EMIT_STR(tmp); break;
        case 'm': snprintf(tmp, sizeof tmp, "%02d", tm->tm_mon + 1); EMIT_STR(tmp); break;
        case 'M': snprintf(tmp, sizeof tmp, "%02d", tm->tm_min); EMIT_STR(tmp); break;
        case 'n': EMIT_STR("\n"); break;
        case 't': EMIT_STR("\t"); break;
        case 'p': EMIT_STR(tm->tm_hour >= 12 ? "PM" : "AM"); break;
        case 'S': snprintf(tmp, sizeof tmp, "%02d", tm->tm_sec); EMIT_STR(tmp); break;
        case 's': { struct tm c = *tm; snprintf(tmp, sizeof tmp, "%d", (int)mktime(&c)); EMIT_STR(tmp); break; }
        case 'T': case 'X': snprintf(tmp, sizeof tmp, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec); EMIT_STR(tmp); break;
        case 'U': snprintf(tmp, sizeof tmp, "%02d", (tm->tm_yday + 7 - tm->tm_wday) / 7); EMIT_STR(tmp); break;
        case 'w': snprintf(tmp, sizeof tmp, "%d", tm->tm_wday); EMIT_STR(tmp); break;
        case 'x': case 'D': snprintf(tmp, sizeof tmp, "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100); EMIT_STR(tmp); break;
        case 'y': snprintf(tmp, sizeof tmp, "%02d", tm->tm_year % 100); EMIT_STR(tmp); break;
        case 'Y': snprintf(tmp, sizeof tmp, "%d", tm->tm_year + 1900); EMIT_STR(tmp); break;
        case 'Z': EMIT_STR("UTC"); break;
        case 'z': EMIT_STR("+0000"); break;
        case '%': EMIT_STR("%"); break;
        default: if (n + 2 >= max) return 0; s[n++] = '%'; s[n++] = *fmt; break;
        }
    }
    if (n >= max) return 0;
    s[n] = 0;
    return n;
#undef EMIT_STR
}

char* asctime(const struct tm* tm) {
    static char buf[32];
    strftime(buf, sizeof buf, "%c\n", tm);
    return buf;
}

char* ctime(const time_t* t) { return asctime(localtime(t)); }
