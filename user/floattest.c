#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

static int bad;
static void chk(const char* got, const char* want) {
    if (strcmp(got, want)) { printf("FAIL: got '%s' want '%s'\n", got, want); bad++; }
}
static char b[128];
#define F(fmt, v, want) do { snprintf(b, sizeof b, fmt, v); chk(b, want); } while (0)

int main(void) {
    F("%f", 3.14159265, "3.141593");
    F("%.2f", 2.675, "2.67");          // exact binary value is below the tie
    F("%.0f", 0.5, "0");
    F("%.0f", 1.5, "2");
    F("%.0f", 2.5, "2");
    F("%f", 0.0, "0.000000");
    F("%.3f", -1.0005, "-1.000");
    F("%8.2f", 3.14159, "    3.14");
    F("%-8.2f|", 3.14159, "3.14    |");
    F("%08.2f", -3.14159, "-0003.14");
    F("%e", 12345.6789, "1.234568e+04");
    F("%.2e", 0.000123456, "1.23e-04");
    F("%E", 1e100, "1.000000E+100");
    F("%g", 100000.0, "100000");
    F("%g", 1000000.0, "1e+06");
    F("%g", 0.0001, "0.0001");
    F("%g", 0.00001, "1e-05");
    F("%g", 3.14159265, "3.14159");
    F("%.14g", 0.1, "0.1");
    F("%.14g", 1.0/3.0, "0.33333333333333");
    F("%.17g", 0.1, "0.10000000000000001");
    F("%.14g", 123456789012345678.0, "1.2345678901235e+17");
    F("%g", 5.0, "5");
    F("%#g", 5.0, "5.00000");
    F("%f", 1e15, "1000000000000000.000000");
    F("%.0f", 1e22, "10000000000000000000000");
    F("%f", 1.0/0.0, "inf");
    F("%f", -1.0/0.0, "-inf");
    F("%.3f", 0.0005, "0.001");        // 0.0005 is slightly above the tie in binary
    F("%.1f", 0.05, "0.1");
    F("%.2f", 1e-10, "0.00");
    F("%g", 1e-300, "1e-300");
    F("%.15g", 5e-324, "4.94065645841247e-324");
    F("%+.1f", 2.0, "+2.0");

    chk((snprintf(b, sizeof b, "%.14g", strtod("0.1", 0)), b), "0.1");
    if (strtod("3.5e2", 0) != 350.0) { printf("FAIL strtod 3.5e2\n"); bad++; }
    if (strtod("-0.25", 0) != -0.25) { printf("FAIL strtod -0.25\n"); bad++; }
    char* e; double d = strtod("12abc", &e);
    if (d != 12.0 || *e != 'a') { printf("FAIL strtod end\n"); bad++; }
    if (atof("1.5") != 1.5) { printf("FAIL atof\n"); bad++; }

    // libm
    F("%.6f", sin(1.0), "0.841471");
    F("%.6f", cos(1.0), "0.540302");
    F("%.6f", tan(1.0), "1.557408");
    F("%.6f", atan2(1.0, 2.0), "0.463648");
    F("%.6f", exp(1.0), "2.718282");
    F("%.6f", log(10.0), "2.302585");
    F("%.6f", pow(2.0, 0.5), "1.414214");
    F("%.1f", pow(2.0, 10.0), "1024.0");
    F("%.6f", sqrt(2.0), "1.414214");
    F("%.1f", floor(-1.5), "-2.0");
    F("%.1f", ceil(-1.5), "-1.0");
    F("%.2f", fmod(10.5, 3.0), "1.50");
    F("%.6f", log10(1000.0), "3.000000");
    F("%.6f", sinh(1.0), "1.175201");
    int ex; double fr = frexp(8.0, &ex);
    if (fr != 0.5 || ex != 4) { printf("FAIL frexp\n"); bad++; }
    if (ldexp(1.0, 10) != 1024.0) { printf("FAIL ldexp\n"); bad++; }

    jmp_buf jb;
    volatile int n = 0;
    int r = setjmp(jb);
    if (r == 0) { n++; longjmp(jb, 7); }
    if (r != 7 || n != 1) { printf("FAIL setjmp r=%d n=%d\n", r, n); bad++; }

    printf("floattest %s\n", bad ? "FAILED" : "ok");
    return bad != 0;
}
