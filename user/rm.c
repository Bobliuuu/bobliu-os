// rm [-rRf] path...
#include "common.h"
static int fr, ff;

static int rm_tree(const char* path) {
    if (!is_dir(path)) return unlink(path) < 0;
    char name[DIRENT_NAME_MAX];
    for (;;) {
        int found = 0;
        for (int i = 0;; i++) {
            int r = readdir_at(path, i, name);
            if (r <= 0) break;
            if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
            char sub[PATH_MAX_];
            path_join(sub, sizeof sub, path, name);
            if (rm_tree(sub)) { err("rm", sub, "cannot remove"); return 1; }
            found = 1;
            break;
        }
        if (!found) break;
    }
    return unlink(path) < 0;
}

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'r' || *p == 'R') fr = 1; else if (*p == 'f') ff = 1;
            else { pf("rm: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (i >= argc) { if (!ff) { pf("rm: missing operand\n"); return 1; } return 0; }
    int rc = 0;
    for (; i < argc; i++) {
        const char* p = argv[i];
        if (!exists(p)) { if (!ff) { err("rm", p, "No such file or directory"); rc = 1; } continue; }
        if (is_dir(p) && !fr) { err("rm", p, "is a directory"); rc = 1; continue; }
        if (rm_tree(p)) { err("rm", p, "cannot remove"); rc = 1; }
    }
    return rc;
}
