// wc [file]: lines words bytes of a file, or of stdin until EOF
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = 0;
    if (argc > 1) {
        fd = open(argv[1], 0);
        if (fd < 0) { printf("wc: %s: cannot open\n", argv[1]); return 1; }
    }
    char buf[256];
    int n, lines = 0, words = 0, bytes = 0, inword = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') lines++;
            if (c == ' ' || c == '\n' || c == '\t') inword = 0;
            else if (!inword) { inword = 1; words++; }
        }
        bytes += n;
    }
    printf("%d %d %d\n", lines, words, bytes);
    return 0;
}
