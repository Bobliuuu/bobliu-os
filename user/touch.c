// touch file...: create empty files that do not exist (no mtime in this os)
#include "common.h"
int main(int argc, char** argv) {
    int rc = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) if (!strcmp(argv[i], "--")) { i++; break; }
    if (i >= argc) { pf("touch: missing file operand\n"); return 1; }
    for (; i < argc; i++) {
        if (exists(argv[i])) continue;
        int fd = open(argv[i], O_WRONLY | O_CREAT);
        if (fd < 0) { err("touch", argv[i], "cannot touch"); rc = 1; } else close(fd);
    }
    return rc;
}
