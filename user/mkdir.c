#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: mkdir <path>...\n"); return 1; }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i]) < 0) { printf("mkdir: %s: cannot create\n", argv[i]); rc = 1; }
    }
    return rc;
}
