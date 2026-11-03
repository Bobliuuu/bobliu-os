// tac [file...]: lines in reverse order
#include "common.h"
int main(int argc, char** argv) {
    int nf = argc - 1, rc = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("tac", nf ? argv[1 + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        int len; char* buf = slurp(fd, &len); char** v;
        if (fd) close(fd);
        int cnt = split_lines(buf, len, &v);
        for (int j = cnt - 1; j >= 0; j--) puts_nl(v[j]);
        free(v); free(buf);
    }
    DONE(rc);
}
