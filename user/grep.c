// grep <text> [file]: print the lines containing text (stdin if no file)
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: grep <text> [file]\n"); return 1; }
    int fd = 0;
    if (argc > 2) {
        fd = open(argv[2], 0);
        if (fd < 0) { printf("grep: %s: cannot open\n", argv[2]); return 1; }
    }
    char line[256], buf[128];
    int len = 0, n, found = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n' || len == (int)sizeof(line) - 1) {
                line[len] = 0;
                if (strstr(line, argv[1])) { printf("%s\n", line); found = 1; }
                len = 0;
                if (buf[i] == '\n') continue;
            }
            line[len++] = buf[i];
        }
    }
    if (len) { line[len] = 0; if (strstr(line, argv[1])) { printf("%s\n", line); found = 1; } }
    return found ? 0 : 1;
}
