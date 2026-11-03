// nl [-ba] [file...]: number lines (non-empty ones only, unless -ba)
#include "common.h"
int main(int argc, char** argv) {
    int all = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-ba")) all = 1;
        else if (!strcmp(argv[i], "-bt")) all = 0;
        else if (!strcmp(argv[i], "-b") && i + 1 < argc) all = argv[++i][0] == 'a';
        else { pf("nl: invalid option %s\n", argv[i]); return 1; }
    }
    int nf = argc - i, rc = 0, no = 1;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("nl", nf ? argv[i + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        Rd r; rd_init(&r, fd);
        char line[1024];
        while (rd_line(&r, line, sizeof line) >= 0) {
            if (line[0] || all) outf("%6d\t%s\n", no++, line);
            else oc('\n');
        }
        if (fd) close(fd);
    }
    DONE(rc);
}
