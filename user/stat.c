// stat file...: name, size, type
#include "common.h"
int main(int argc, char** argv) {
    if (argc < 2) { pf("stat: missing operand\n"); return 1; }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) { err("stat", argv[i], "No such file or directory"); rc = 1; continue; }
        outf("  File: %s\n  Size: %u\t\tType: %s\n", argv[i], st.st_size, st.st_isdir ? "directory" : "regular file");
    }
    DONE(rc);
}
