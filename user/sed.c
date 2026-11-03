// sed [-n] [-e SCRIPT] [SCRIPT] [file...]
// commands: s/re/repl/[gpi]  p  d  q  y/abc/xyz/ ; addresses: N  $  /re/  N,M  /re/,/re/  with optional !
// regex: . * ^ $ [set] \c ; replacement: & \n \\ \&  (no groups / \1)
#include "common.h"
#define MAXC 32
struct cmd {
    char c, neg, g, p, icase;
    char t1, t2;            // 0 none, 'n' line, '$' last, '/' regex
    int n1, n2, inrange;
    char *r1, *r2, *re, *repl;
    char ymap[256];
};
static struct cmd cmds[MAXC];
static int ncmd;

static char* take(const char** s, char delim) {   // up to unescaped delim; \delim -> delim
    char* o = malloc(strlen(*s) + 1);
    int n = 0;
    const char* p = *s;
    while (*p && *p != delim) {
        if (*p == '\\' && p[1]) {
            if (p[1] == delim) { o[n++] = delim; p += 2; continue; }
            o[n++] = *p++;
        }
        o[n++] = *p++;
    }
    o[n] = 0;
    if (*p == delim) p++;
    *s = p;
    return o;
}
static int addr(const char** s, char* t, int* n, char** re) {
    const char* p = *s;
    if (isdigit((unsigned char)*p)) { *t = 'n'; *n = strtol(p, (char**)&p, 10); }
    else if (*p == '$') { *t = '$'; p++; }
    else if (*p == '/') { p++; *t = '/'; *re = take(&p, '/'); }
    else return 0;
    *s = p;
    return 1;
}
static int parse(const char* s) {
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == ';' || *s == '\n') s++;
        if (!*s) break;
        if (ncmd == MAXC) return -1;
        struct cmd* c = &cmds[ncmd];
        memset(c, 0, sizeof *c);
        if (addr(&s, &c->t1, &c->n1, &c->r1) && *s == ',') { s++; if (!addr(&s, &c->t2, &c->n2, &c->r2)) return -1; }
        while (*s == ' ') s++;
        if (*s == '!') { c->neg = 1; s++; while (*s == ' ') s++; }
        c->c = *s;
        if (!*s) return -1;
        s++;
        if (c->c == 's') {
            char d = *s;
            if (!d || d == '\\' || d == '\n') return -1;
            s++;
            c->re = take(&s, d);
            c->repl = take(&s, d);
            for (; *s && *s != ';' && *s != '\n' && *s != ' '; s++) {
                if (*s == 'g') c->g = 1; else if (*s == 'p') c->p = 1; else if (*s == 'i' || *s == 'I') c->icase = 1; else return -1;
            }
        } else if (c->c == 'y') {
            char d = *s++;
            char* a = take(&s, d); char* b = take(&s, d);
            if (strlen(a) != strlen(b)) return -1;
            for (int i = 0; i < 256; i++) c->ymap[i] = (char)i;
            for (int i = 0; a[i]; i++) c->ymap[(unsigned char)a[i]] = b[i];
        } else if (c->c != 'p' && c->c != 'd' && c->c != 'q') return -1;
        ncmd++;
    }
    return 0;
}

static int matches(struct cmd* c, int ln, int last, const char* ps) {
    int r;
    if (!c->t1) r = 1;
    else {
        int m1 = c->t1 == 'n' ? ln == c->n1 : c->t1 == '$' ? last : re_test(c->r1, ps);
        if (!c->t2) r = m1;
        else if (c->inrange) {
            r = 1;
            if (c->t2 == 'n' ? ln >= c->n2 : c->t2 == '$' ? last : re_test(c->r2, ps)) c->inrange = 0;
        } else if (m1) {
            r = 1;
            if (!(c->t2 == 'n' ? c->n2 <= ln : c->t2 == '$' ? last : 0)) c->inrange = 1;
        } else r = 0;
    }
    return r != c->neg;
}

static int subst(struct cmd* c, char* ps) {
    char out[2048];
    int o = 0, did = 0;
    const char* cur = ps;
    const char* prevend = 0;
    re_icase = c->icase;
    for (;;) {
        const char *ms, *me;
        if (!re_search(c->re, ps, cur, &ms, &me)) break;
        if (me == ms && ms == prevend) {   // no empty match right after a real one
            if (!*ms) { cur = ms; break; }
            out[o++] = *ms; cur = ms + 1; prevend = 0;
            continue;
        }
        did = 1;
        for (const char* q = cur; q < ms && o < 2000; q++) out[o++] = *q;
        for (const char* r = c->repl; *r && o < 2000; r++) {
            if (*r == '&') { for (const char* q = ms; q < me && o < 2000; q++) out[o++] = *q; }
            else if (*r == '\\' && r[1]) { r++; out[o++] = *r == 'n' ? '\n' : *r == 't' ? '\t' : *r; }
            else out[o++] = *r;
        }
        if (me == ms) {
            if (!*ms) { cur = ms; break; }
            out[o++] = *ms; cur = ms + 1;
        } else { cur = me; prevend = me; }
        if (!c->g) break;
    }
    if (!did) return 0;
    for (; *cur && o < 2047; cur++) out[o++] = *cur;
    out[o] = 0;
    strcpy(ps, out);
    return 1;
}

int main(int argc, char** argv) {
    int nflag = 0, i = 1, have = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-n")) nflag = 1;
        else if (!strcmp(argv[i], "-e") && i + 1 < argc) { if (parse(argv[++i]) < 0) { pf("sed: bad script\n"); return 1; } have = 1; }
        else if (!strcmp(argv[i], "-E") || !strcmp(argv[i], "-r")) re_ere = 1;
        else { pf("sed: unknown option %s\n", argv[i]); return 1; }
    }
    if (!have) {
        if (i >= argc) { pf("usage: sed [-n] [-e] script [file...]\n"); return 1; }
        if (parse(argv[i++]) < 0) { pf("sed: bad script\n"); return 1; }
    }
    int nf = argc - i, rc = 0, ln = 0, quit = 0, k = 0;
    int fd = -2;
    Rd r;
    char cur[1100], nxt[1100];
    int have_cur = 0, eof = 0;
    // fetch lines across files with one line of lookahead
    #define FETCH(dst) ({ int got = 0; while (!eof) { if (fd == -2) { if (k >= (nf ? nf : 1)) { eof = 1; break; } \
        fd = open_in("sed", nf ? argv[i + k] : "-"); k++; if (fd < 0) { rc = 1; fd = -2; continue; } rd_init(&r, fd); } \
        if (rd_line(&r, dst, 1024) >= 0) { got = 1; break; } if (fd) close(fd); fd = -2; } got; })
    have_cur = FETCH(cur);
    while (have_cur && !quit) {
        int have_nxt = FETCH(nxt);
        ln++;
        int del = 0;
        for (int j = 0; j < ncmd && !del && !quit; j++) {
            struct cmd* c = &cmds[j];
            if (!matches(c, ln, !have_nxt, cur)) continue;
            switch (c->c) {
            case 'p': puts_nl(cur); break;
            case 'd': del = 1; break;
            case 'q': quit = 1; break;
            case 's': if (subst(c, cur) && c->p) puts_nl(cur); break;
            case 'y': for (char* q = cur; *q; q++) *q = c->ymap[(unsigned char)*q]; break;
            }
        }
        if (!del && !nflag) puts_nl(cur);
        if (!have_nxt) break;
        memcpy(cur, nxt, sizeof cur);
    }
    DONE(rc);
}
