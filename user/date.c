// date [-u] [+FORMAT]   %Y %y %m %d %e %H %M %S %s %F %T %D %a %A %b %B %j %Z %n %t %%
#include "common.h"
#include <time.h>
static const char* DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char* MONS[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

static void ps_(const char* s) { put(s, strlen(s)); }
static void fmt(const char* f, const struct tm* t, time_t now) {
    for (; *f; f++) {
        if (*f != '%' || !f[1]) { oc(*f); continue; }
        switch (*++f) {
        case 'Y': outf("%d", t->tm_year + 1900); break;
        case 'y': outf("%02d", t->tm_year % 100); break;
        case 'm': outf("%02d", t->tm_mon + 1); break;
        case 'd': outf("%02d", t->tm_mday); break;
        case 'e': outf("%2d", t->tm_mday); break;
        case 'H': outf("%02d", t->tm_hour); break;
        case 'I': outf("%02d", t->tm_hour % 12 ? t->tm_hour % 12 : 12); break;
        case 'p': put(t->tm_hour < 12 ? "AM" : "PM", 2); break;
        case 'M': outf("%02d", t->tm_min); break;
        case 'S': outf("%02d", t->tm_sec); break;
        case 's': outf("%d", (int)now); break;
        case 'j': outf("%03d", t->tm_yday + 1); break;
        case 'F': outf("%d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday); break;
        case 'T': outf("%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec); break;
        case 'D': outf("%02d/%02d/%02d", t->tm_mon + 1, t->tm_mday, t->tm_year % 100); break;
        case 'a': put(DAYS[t->tm_wday], 3); break;
        case 'A': ps_(DAYS[t->tm_wday]); break;
        case 'b': case 'h': put(MONS[t->tm_mon], 3); break;
        case 'B': ps_(MONS[t->tm_mon]); break;
        case 'Z': put("UTC", 3); break;
        case 'z': put("+0000", 5); break;
        case 'n': oc('\n'); break;
        case 't': oc('\t'); break;
        case '%': oc('%'); break;
        default: oc('%'); oc(*f);
        }
    }
}

int main(int argc, char** argv) {
    const char* f = "%a %b %e %H:%M:%S %Z %Y";
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '+') f = argv[i] + 1;
        else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--utc")) ;
        else { pf("date: invalid date '%s'\n", argv[i]); return 1; }
    }
    time_t now = time(0);
    struct tm tm;
    gmtime_r(&now, &tm);
    fmt(f, &tm, now);
    oc('\n');
    DONE(0);
}
