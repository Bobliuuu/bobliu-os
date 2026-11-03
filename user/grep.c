// grep [-ivncl LhHqrFxE] [-e PATTERN] PATTERN [file...]
// basic regex: . * ^ $ [set] \c ; with -E also + ? and a|b alternation (no groups)
#include "common.h"
static int fi, fv, fn, fc, fl, fL, fh, fH, fq, fr, fF, fx;
static char* alts[8];
static int nalt;
static int any;

static int line_match(const char* line) {
    for (int a = 0; a < nalt; a++) {
        const char *s, *e;
        if (fF) {
            int pl = strlen(alts[a]);
            for (const char* t = line;; t++) {
                int ok = re_icase ? strncasecmp(t, alts[a], pl) == 0 : strncmp(t, alts[a], pl) == 0;
                if (ok && (!fx || (t == line && !t[pl]))) return 1;
                if (!*t) break;
            }
        } else if (fx) {
            if (re_search(alts[a], line, line, &s, &e) && s == line && !*e) return 1;
            // x-match needs the whole line: retry anchored
            char tmp[1100];
            snprintf(tmp, sizeof tmp, "^%s$", alts[a]);
            if (re_test(tmp, line)) return 1;
        } else if (re_search(alts[a], line, line, &s, &e)) return 1;
    }
    return 0;
}

static int grep_fd(int fd, const char* name, int prefix) {
    Rd r; rd_init(&r, fd);
    char line[1024];
    int lineno = 0, count = 0;
    while (rd_line(&r, line, sizeof line) >= 0) {
        lineno++;
        if (line_match(line) == fv) continue;
        count++; any = 1;
        if (fq || fl || fL) break;
        if (fc) continue;
        if (prefix) outf("%s:", name);
        if (fn) outf("%d:", lineno);
        puts_nl(line);
    }
    if (fc && !fq) { if (prefix) outf("%s:", name); outf("%d\n", count); }
    if (fl && count) puts_nl(name);
    if (fL && !count) puts_nl(name);
    return count;
}

static void grep_path(const char* path, int prefix, int top) {
    if (is_dir(path)) {
        if (!fr) { err("grep", path, "Is a directory"); return; }
        char name[DIRENT_NAME_MAX];
        for (int i = 0;; i++) {
            if (readdir_at(path, i, name) <= 0) break;
            if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
            char sub[PATH_MAX_];
            if (top == 2) strcpy(sub, name); else path_join(sub, sizeof sub, path, name);
            grep_path(sub, 1, 1);
        }
        return;
    }
    int fd = open_in("grep", path);
    if (fd < 0) return;
    grep_fd(fd, fd == 0 ? "(standard input)" : path, prefix);
    if (fd) close(fd);
}

static void add_pattern(char* p) {
    if (re_ere && !fF) {
        char* s = p;
        for (char* q = p;; q++) {
            if (*q == '\\' && q[1]) { q++; continue; }
            if (*q == '|' || !*q) {
                char end = *q;
                *q = 0;
                if (nalt < 8) alts[nalt++] = s;
                s = q + 1;
                if (!end) break;
            }
        }
    } else if (nalt < 8) alts[nalt++] = p;
}

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            switch (*p) {
            case 'i': fi = 1; break; case 'v': fv = 1; break; case 'n': fn = 1; break;
            case 'c': fc = 1; break; case 'l': fl = 1; break; case 'L': fL = 1; break;
            case 'h': fh = 1; break; case 'H': fH = 1; break; case 'q': fq = 1; break;
            case 'r': case 'R': fr = 1; break; case 'F': fF = 1; break; case 'x': fx = 1; break;
            case 'E': re_ere = 1; break; case 'G': case 's': break;
            case 'e':
                if (p[1]) { add_pattern(strdup(p + 1)); p += strlen(p) - 1; }
                else if (i + 1 < argc) add_pattern(strdup(argv[++i]));
                break;
            default: pf("grep: invalid option -- '%c'\n", *p); return 2;
            }
        }
    }
    re_icase = fi;
    if (!nalt) {
        if (i >= argc) { pf("usage: grep [OPTION]... PATTERN [FILE]...\n"); return 2; }
        add_pattern(strdup(argv[i++]));
    }
    int nf = argc - i;
    int prefix = (nf > 1 || fr) && !fh;
    if (fH) prefix = 1;
    if (nf == 0) {
        if (fr) grep_path(".", 1, 2);
        else grep_fd(0, "(standard input)", fH);
    }
    for (; i < argc; i++) grep_path(argv[i], prefix, 0);
    DONE(any ? 0 : 1);
}
