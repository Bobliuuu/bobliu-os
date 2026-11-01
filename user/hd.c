// hd <file> [offset] [len]: hex dump, default first 128 bytes
#include <stdio.h>
#include <unistd.h>

static int num(const char* s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return v;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: hd <file> [offset] [len]\n"); return 1; }
    int fd = open(argv[1], 0);
    if (fd < 0) { printf("hd: %s: cannot open\n", argv[1]); return 1; }
    int off = argc > 2 ? num(argv[2]) : 0;
    int len = argc > 3 ? num(argv[3]) : 128;
    if (off && lseek(fd, off, SEEK_SET) < 0) { printf("hd: bad offset\n"); return 1; }

    unsigned char buf[16];
    for (int done = 0; done < len;) {
        int want = len - done < 16 ? len - done : 16;
        int n = read(fd, buf, want);
        if (n <= 0) break;
        printf("%x: ", off + done);
        for (int i = 0; i < n; i++) printf("%x%x ", buf[i] >> 4, buf[i] & 15);
        for (int i = n; i < 16; i++) printf("   ");
        for (int i = 0; i < n; i++) putchar(buf[i] >= 32 && buf[i] < 127 ? buf[i] : '.');
        putchar('\n');
        done += n;
    }
    return 0;
}
