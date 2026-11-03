// cp [-rRaf] src... dst
#include "common.h"
static int fr;

static int cp1(const char* src, const char* dst) {
    if (!exists(src)) { err("cp", src, "No such file or directory"); return 1; }
    char to[PATH_MAX_];
    if (is_dir(dst)) { path_join(to, sizeof to, dst, base_of(src)); dst = to; }
    if (!strcmp(src, dst)) { err("cp", src, "same file"); return 1; }
    if (is_dir(src)) {
        if (!fr) { err("cp", src, "-r not specified; omitting directory"); return 1; }
        int sl = strlen(src);
        if (!strncmp(dst, src, sl) && (dst[sl] == '/' || !dst[sl])) { err("cp", src, "cannot copy a directory into itself"); return 1; }
        if (!is_dir(dst) && mkdir(dst) < 0) { err("cp", dst, "cannot create directory"); return 1; }
        char name[DIRENT_NAME_MAX];
        int rc = 0;
        for (int i = 0;; i++) {
            int r = readdir_at(src, i, name);
            if (r <= 0) break;
            if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
            char s[PATH_MAX_], d[PATH_MAX_];
            path_join(s, sizeof s, src, name);
            path_join(d, sizeof d, dst, name);
            rc |= cp1(s, d);
        }
        return rc;
    }
    int r = copy_file(src, dst);
    if (r == -1) { err("cp", src, "cannot open"); return 1; }
    if (r == -2) { err("cp", dst, "cannot create regular file"); return 1; }
    if (r < 0) { err("cp", dst, "write error"); return 1; }
    return 0;
}

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'r' || *p == 'R' || *p == 'a') fr = 1; else if (*p == 'f') ;
            else { pf("cp: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    int n = argc - i;
    if (n < 2) { pf("cp: missing file operand\n"); return 1; }
    const char* dst = argv[argc - 1];
    if (n > 2 && !is_dir(dst)) { err("cp", dst, "not a directory"); return 1; }
    int rc = 0;
    for (; i < argc - 1; i++) rc |= cp1(argv[i], dst);
    DONE(rc);
}
