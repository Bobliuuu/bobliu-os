#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: cat <file>...\n"); return 1; }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], 0);
        if (fd < 0) { printf("cat: %s: cannot open\n", argv[i]); rc = 1; continue; }
        char buf[256];
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) write(1, buf, n);
        close(fd);
    }
    return rc;
}
