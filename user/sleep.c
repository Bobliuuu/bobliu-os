#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: sleep <seconds>\n"); return 1; }
    int n = 0;
    for (const char* c = argv[1]; *c >= '0' && *c <= '9'; c++) n = n * 10 + (*c - '0');
    return sleep((unsigned)n) ? 1 : 0;
}
