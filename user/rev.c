// rev [file...]: reverse each line
#include "common.h"
int main(int argc, char** argv) {
    int nf = argc - 1, rc = 0;
    for (int k = 0; k < (nf ? nf : 1); k++) {
        int fd = open_in("rev", nf ? argv[1 + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        Rd r; rd_init(&r, fd);
        char line[1024];
        int n;
        while ((n = rd_line(&r, line, sizeof line)) >= 0) {
            for (int j = n - 1; j >= 0; j--) oc(line[j]);
            oc('\n');
        }
        if (fd) close(fd);
    }
    DONE(rc);
}
