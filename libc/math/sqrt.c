// x87 fsqrt (correctly rounded for double on i686)
#include <math.h>
double sqrt(double x) {
    double r;
    __asm__ ("fsqrt" : "=t"(r) : "0"(x));
    return r;
}
