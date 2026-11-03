// ls [-1alRFd] [path...]   sorted, one entry per line
#include "common.h"
static int fa, fl, fR, fF, fd_;

static int cmp_s(const void* a, const void* b) { return strcmp(*(char* const*)a, *(char* const*)b); }

static void show(const char* dir, const char* name) {
    struct stat st;
    char full[PATH_MAX_];
    if (dir) path_join(full, sizeof full, dir, name); else strcpy(full, name);
    int ok = stat(full, &st) == 0;
    if (fl) outf("%c %8u ", ok && st.st_isdir ? 'd' : '-', ok ? st.st_size : 0);
    outf("%s", name);
    if (fF && ok && st.st_isdir) oc('/');
    oc('\n');
}

static int list_dir(const char* dir, int header) {
    char name[DIRENT_NAME_MAX];
    int cap = 64, n = 0;
    char** v = malloc(sizeof(char*) * cap);
    for (int i = 0;; i++) {
        int r = readdir_at(dir, i, name);
        if (r < 0) { err("ls", dir, "cannot access"); free(v); return 1; }
        if (r == 0) break;
        if (name[0] == '.' && !fa) continue;
        if (n == cap) { cap *= 2; v = realloc(v, sizeof(char*) * cap); }
        v[n++] = strdup(name);
    }
    if (fa) {   // readdir does not list them
        int hd = 0, hdd = 0;
        for (int i = 0; i < n; i++) { if (!strcmp(v[i], ".")) hd = 1; if (!strcmp(v[i], "..")) hdd = 1; }
        if (n + 2 >= cap) { cap += 2; v = realloc(v, sizeof(char*) * cap); }
        if (!hd) v[n++] = strdup(".");
        if (!hdd) v[n++] = strdup("..");
    }
    qsort(v, n, sizeof(char*), cmp_s);
    if (header) outf("%s:\n", dir);
    for (int i = 0; i < n; i++) show(dir, v[i]);
    int rc = 0;
    if (fR)
        for (int i = 0; i < n; i++) {
            if (!strcmp(v[i], ".") || !strcmp(v[i], "..")) continue;
            char sub[PATH_MAX_];
            path_join(sub, sizeof sub, dir, v[i]);
            if (is_dir(sub)) { oc('\n'); rc |= list_dir(sub, 1); }
        }
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
    return rc;
}

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'a' || *p == 'A') fa = 1; else if (*p == 'l') fl = 1; else if (*p == 'R') fR = 1;
            else if (*p == 'F') fF = 1; else if (*p == 'd') fd_ = 1; else if (*p == '1') ;
            else { pf("ls: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    int n = argc - i, rc = 0;
    if (n == 0) { rc = list_dir(".", fR && 0); DONE(rc); }
    int shown = 0;
    for (int k = i; k < argc; k++) {           // files (and -d dirs) first
        if (!exists(argv[k])) { err("ls", argv[k], "No such file or directory"); rc = 1; }
        else if (!is_dir(argv[k]) || fd_) { show(0, argv[k]); shown = 1; }
    }
    for (int k = i; k < argc; k++) {
        if (!exists(argv[k]) || !is_dir(argv[k]) || fd_) continue;
        if (shown) oc('\n');
        rc |= list_dir(argv[k], n > 1 || fR);
        shown = 1;
    }
    DONE(rc);
}
