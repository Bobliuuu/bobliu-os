// tee [-a] file...
#include "common.h"
int main(int argc, char** argv) {
    int app = 0, i = 1, fds[16], nf = 0, rc = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-a")) app = 1; else { pf("tee: invalid option %s\n", argv[i]); return 1; }
    }
    for (; i < argc && nf < 16; i++) {
        int fd = open(argv[i], O_WRONLY | O_CREAT | (app ? O_APPEND : O_TRUNC));
        if (fd < 0) { err("tee", argv[i], "cannot open"); rc = 1; } else fds[nf++] = fd;
    }
    char buf[512]; int n;
    while ((n = read(0, buf, sizeof buf)) > 0) {
        write(1, buf, n);
        for (int k = 0; k < nf; k++) write(fds[k], buf, n);
    }
    for (int k = 0; k < nf; k++) close(fds[k]);
    return rc;
}
