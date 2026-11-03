// cat [-n] [-b] [-s] [file...]   "-" or no file = stdin
#include "common.h"
int main(int argc, char** argv) {
    int num = 0, nb = 0, sq = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'n') num = 1; else if (*p == 'b') nb = num = 1; else if (*p == 's') sq = 1;
            else { pf("cat: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    int nf = argc - i, rc = 0, no = 1, blank = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("cat", nf ? argv[i + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        if (!num && !sq) {
            char buf[1024];
            int n;
            oflush();
            while ((n = read(fd, buf, sizeof buf)) > 0) write(1, buf, n);
        } else {
            Rd r; rd_init(&r, fd);
            char line[1024];
            while (rd_line(&r, line, sizeof line) >= 0) {
                if (!line[0]) { if (sq && blank) continue; blank = 1; } else blank = 0;
                if (num && !(nb && !line[0])) outf("%6d\t", no++);
                puts_nl(line);
                oflush();
            }
        }
        if (fd) close(fd);
    }
    DONE(rc);
}
