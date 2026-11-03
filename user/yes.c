#include "common.h"
int main(int argc, char** argv) {
    char line[256]; int n = 0;
    if (argc < 2) { strcpy(line, "y"); n = 1; }
    for (int i = 1; i < argc; i++) { if (i > 1) line[n++] = ' '; for (const char* p = argv[i]; *p && n < 250; p++) line[n++] = *p; }
    line[n++] = '\n';
    char buf[512]; int b = 0;
    while (b + n <= (int)sizeof buf) { memcpy(buf + b, line, n); b += n; }
    for (;;) if (write(1, buf, b) < 0) return 0;
}
