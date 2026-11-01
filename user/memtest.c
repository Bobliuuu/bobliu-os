#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int fail(const char* what) {
    printf("memtest FAIL: %s\n", what);
    return 1;
}

int main(void) {
    // raw brk
    char* b0 = sbrk(0);
    char* p = sbrk(3 * 4096);
    if (p != b0) return fail("sbrk returns old break");
    p[0] = 1; p[3 * 4096 - 1] = 2;
    if (p[1] != 0 || p[5000] != 0) return fail("heap pages not zeroed");
    if (sbrk(-4096) == (void*)-1) return fail("shrink");
    if (sbrk(0) != b0 + 2 * 4096) return fail("break after shrink");
    if (brk((void*)0x10) != -1) return fail("bad brk accepted");
    printf("brk/sbrk: ok\n");

    // malloc basics
    char* a = malloc(100);
    char* b = malloc(5000);
    char* c = malloc(1);
    if (!a || !b || !c) return fail("malloc null");
    memset(a, 'a', 100); memset(b, 'b', 5000); *c = 'c';
    if (a[99] != 'a' || b[4999] != 'b' || *c != 'c') return fail("malloc overlap");
    if (((unsigned)a & 7) || ((unsigned)b & 7)) return fail("alignment");
    free(b);
    char* b2 = malloc(4000);              // should reuse b's hole
    if (b2 != b) return fail("hole reuse");
    free(a); free(b2); free(c);
    char* big = malloc(200000);           // forces a lot of growth
    if (!big) return fail("big malloc");
    big[0] = 1; big[199999] = 2;
    free(big);
    printf("malloc/free: ok\n");

    // calloc + realloc keep contents
    int* z = calloc(64, sizeof(int));
    for (int i = 0; i < 64; i++) if (z[i]) return fail("calloc not zero");
    for (int i = 0; i < 64; i++) z[i] = i;
    z = realloc(z, 4096 * sizeof(int));
    for (int i = 0; i < 64; i++) if (z[i] != i) return fail("realloc lost data");
    free(z);
    printf("calloc/realloc: ok\n");

    // lots of small allocations, freed in a scrambled order, must coalesce
    void* v[200];
    char* top0 = sbrk(0);
    for (int i = 0; i < 200; i++) { v[i] = malloc(24 + i); memset(v[i], i, 24 + i); }
    for (int i = 0; i < 200; i += 2) free(v[i]);
    for (int i = 1; i < 200; i += 2) free(v[i]);
    void* all = malloc(8000);             // fits only if everything merged
    if (!all || sbrk(0) != top0) return fail("coalescing");
    free(all);
    printf("coalescing: ok\n");

    // heap is cow across fork
    int* h = malloc(sizeof(int));
    *h = 10;
    int pid = fork();
    if (pid == 0) { *h = 99; exit(*h == 99 ? 0 : 1); }
    int st;
    waitpid(pid, &st, 0);
    if (*h != 10 || WEXITSTATUS(st) != 0) return fail("heap cow");
    printf("heap cow after fork: ok\n");

    // memory returns when a child that grew its heap exits
    for (int i = 0; i < 30; i++) {
        pid = fork();
        if (pid == 0) { char* m = malloc(300000); memset(m, 1, 300000); exit(0); }
        waitpid(pid, &st, 0);
    }
    printf("child heaps freed: ok\n");
    printf("memtest done\n");
    return 0;
}
