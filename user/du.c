// du [-s] [-h] [-a] [path...]  sizes in KiB
#include "common.h"
static int fs, fh, fa;

static void pr(unsigned bytes, const char* path) {
    unsigned k = (bytes + 1023) / 1024;
    if (fh) {
        if (k < 1024) outf("%uK\t%s\n", k, path);
        else if (k < 1024 * 1024) outf("%u.%uM\t%s\n", k / 1024, (k % 1024) * 10 / 1024, path);
        else outf("%u.%uG\t%s\n", k / 1048576, (k / 1024 % 1024) * 10 / 1024, path);
    } else outf("%u\t%s\n", k, path);
}

static unsigned walk(const char* path, int top) {
    struct stat st;
    if (stat(path, &st) < 0) { err("du", path, "No such file or directory"); return 0; }
    if (!st.st_isdir) {
        if ((fa && !fs) || top) { if (!fs || top) pr(st.st_size, path); }
        return st.st_size;
    }
    unsigned total = 0;
    char name[DIRENT_NAME_MAX];
    for (int i = 0;; i++) {
        if (readdir_at(path, i, name) <= 0) break;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        char sub[PATH_MAX_];
        path_join(sub, sizeof sub, path, name);
        total += walk(sub, 0);
    }
    if (!fs || top) pr(total, path);
    return total;
}

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 's') fs = 1; else if (*p == 'h') fh = 1; else if (*p == 'a') fa = 1; else if (*p == 'k') ;
            else { pf("du: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (i >= argc) walk(".", 1);
    for (; i < argc; i++) walk(argv[i], 1);
    DONE(0);
}
