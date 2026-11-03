// mv [-f] src... dst: copy then unlink (files only)
#include "common.h"
static int mv1(const char* src, const char* dst) {
    if (is_dir(src)) { err("mv", src, "is a directory (not supported)"); return 1; }
    char to[PATH_MAX_];
    if (is_dir(dst)) { path_join(to, sizeof to, dst, base_of(src)); dst = to; }
    if (!strcmp(src, dst)) { err("mv", src, "same file"); return 1; }
    int r = copy_file(src, dst);
    if (r == -1) { err("mv", src, "No such file or directory"); return 1; }
    if (r < 0) { err("mv", dst, "cannot create"); return 1; }
    if (unlink(src) < 0) { err("mv", src, "cannot remove"); return 1; }
    return 0;
}
int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) if (!strcmp(argv[i], "--")) { i++; break; }
    int n = argc - i;
    if (n < 2) { pf("mv: missing file operand\n"); return 1; }
    const char* dst = argv[argc - 1];
    if (n > 2 && !is_dir(dst)) { err("mv", dst, "not a directory"); return 1; }
    int rc = 0;
    for (; i < argc - 1; i++) rc |= mv1(argv[i], dst);
    return rc;
}
