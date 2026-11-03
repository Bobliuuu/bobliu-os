// find [path...] [-name GLOB] [-iname GLOB] [-type f|d] [-maxdepth N] [-mindepth N] [-print]
#include "common.h"
static const char* pat;
static int ipat, type, maxd = 1 << 30, mind;
static int rc;

static void visit(const char* path, const char* base, int depth) {
    struct stat st;
    if (stat(path, &st) < 0) { err("find", path, "No such file or directory"); rc = 1; return; }
    int ok = depth >= mind;
    if (ok && pat) {
        if (ipat) {   // case-insensitive: lowercase both
            char a[DIRENT_NAME_MAX], b[DIRENT_NAME_MAX];
            int k;
            for (k = 0; pat[k] && k < DIRENT_NAME_MAX - 1; k++) a[k] = tolower(pat[k]);
            a[k] = 0;
            for (k = 0; base[k] && k < DIRENT_NAME_MAX - 1; k++) b[k] = tolower(base[k]);
            b[k] = 0;
            ok = glob_match(a, b);
        } else ok = glob_match(pat, base);
    }
    if (ok && type == 'f' && st.st_isdir) ok = 0;
    if (ok && type == 'd' && !st.st_isdir) ok = 0;
    if (ok) puts_nl(path);
    if (!st.st_isdir || depth >= maxd) return;
    char name[DIRENT_NAME_MAX];
    for (int i = 0;; i++) {
        if (readdir_at(path, i, name) <= 0) break;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        char sub[PATH_MAX_];
        path_join(sub, sizeof sub, path, name);
        visit(sub, name, depth + 1);
    }
}

int main(int argc, char** argv) {
    int i = 1, np = 0;
    const char* paths[16];
    for (; i < argc && argv[i][0] != '-' && np < 16; i++) paths[np++] = argv[i];
    for (; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "-print")) continue;
        if (i + 1 >= argc) { pf("find: missing argument to '%s'\n", a); return 1; }
        if (!strcmp(a, "-name")) pat = argv[++i];
        else if (!strcmp(a, "-iname")) { pat = argv[++i]; ipat = 1; }
        else if (!strcmp(a, "-type")) type = argv[++i][0];
        else if (!strcmp(a, "-maxdepth")) maxd = atoi(argv[++i]);
        else if (!strcmp(a, "-mindepth")) mind = atoi(argv[++i]);
        else { pf("find: unknown predicate '%s'\n", a); return 1; }
    }
    if (!np) paths[np++] = ".";
    for (int k = 0; k < np; k++) visit(paths[k], base_of(paths[k]), 0);
    DONE(rc);
}
