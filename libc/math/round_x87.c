// floor ceil trunc rint round via x87 frndint with the rounding mode set per call
// (musl's generic versions add and subtract 2^52, which misbehaves with the x87 precision modes)
#include <math.h>

static double rnd(double x, unsigned short rc) {
    unsigned short cw, cw2;
    double r;
    __asm__ volatile ("fnstcw %0" : "=m"(cw));
    cw2 = (unsigned short)((cw & ~0xC00) | rc);
    __asm__ volatile ("fldcw %0" : : "m"(cw2));
    __asm__ volatile ("frndint" : "=t"(r) : "0"(x));
    __asm__ volatile ("fldcw %0" : : "m"(cw));
    return r;
}

double floor(double x) { return rnd(x, 0x400); }
double ceil(double x)  { return rnd(x, 0x800); }
double trunc(double x) { return rnd(x, 0xC00); }
double rint(double x)  { return rnd(x, 0x000); }

double round(double x) {
    double t = trunc(x);
    if (fabs(x - t) >= 0.5) t += copysign(1.0, x);
    return t;
}
