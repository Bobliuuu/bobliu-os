#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 3) { printf("usage: cp <src> <dst>\n"); return 1; }
    int in = open(argv[1], O_RDONLY);
    if (in < 0) { printf("cp: %s: cannot open\n", argv[1]); return 1; }
    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) { printf("cp: %s: cannot create\n", argv[2]); close(in); return 1; }

    char buf[512];
    int n, rc = 0;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, n) != n) { printf("cp: write failed\n"); rc = 1; break; }
    }
    close(in);
    close(out);
    return rc;
}
