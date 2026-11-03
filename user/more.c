// more [file...]: page output 24 lines at a time. With file arguments the keyboard is fd 0:
// space = next page, enter = next line, q = quit. Reading from a pipe cannot pause (the
// keyboard is not reachable), so it just prints everything.
#include "common.h"
#define ROWS 24
int main(int argc, char** argv) {
    int nf = argc - 1, rc = 0, interactive = nf > 0;
    if (interactive) tcsetraw(1);
    int lines = 0, quit = 0;
    for (int k = 0; k < (nf ? nf : 1) && !quit; k++) {
        int fd = open_in("more", nf ? argv[1 + k] : "-");
        if (fd < 0) { rc = 1; continue; }
        Rd r; rd_init(&r, fd);
        char line[1024];
        int n;
        while ((n = rd_line(&r, line, sizeof line)) >= 0) {
            if (interactive && lines >= ROWS - 1) {
                oflush();
                printf("--More--");
                char c;
                for (;;) {
                    if (read(0, &c, 1) <= 0) { quit = 1; break; }
                    if (c == ' ') { lines = 0; break; }
                    if (c == '\n' || c == '\r') { lines = ROWS - 2; break; }
                    if (c == 'q' || c == 'Q' || c == 3) { quit = 1; break; }
                }
                printf("\r        \r");
                if (quit) break;
            }
            put(line, n);
            oc('\n');
            lines += 1 + n / 80;
        }
        if (fd) close(fd);
    }
    oflush();
    if (interactive) tcsetraw(0);
    return rc;
}
