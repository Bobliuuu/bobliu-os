// mkdir [-p] dir...
#include "common.h"
int main(int argc, char** argv) {
    int par = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'p') par = 1; else { pf("mkdir: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (i >= argc) { pf("mkdir: missing operand\n"); return 1; }
    int rc = 0;
    for (; i < argc; i++) {
        char path[PATH_MAX_];
        strncpy(path, argv[i], sizeof path - 1); path[sizeof path - 1] = 0;
        if (par) {
            for (char* p = path + 1; *p; p++) {
                if (*p != '/') continue;
                *p = 0;
                if (!exists(path)) mkdir(path);
                *p = '/';
            }
            if (is_dir(path)) continue;
        }
        if (exists(path)) { err("mkdir", argv[i], "File exists"); rc = 1; }
        else if (mkdir(path) < 0) { err("mkdir", argv[i], "cannot create directory"); rc = 1; }
    }
    return rc;
}
