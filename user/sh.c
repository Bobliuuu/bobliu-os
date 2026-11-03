// bobliu user-space shell.
//   interactive: raw-mode line editor, history, tab completion, ctrl-l
//   language:    pipelines, ; && || &, < > >> 2> 2>> 2>&1 >&2, 'quotes' "quotes" \escapes,
//                $VAR ${VAR} $? $$ $#, ~, globbing (* ? [..] in the last path component),
//                VAR=value, if/elif/else/fi, while/until/for/do/done, break/continue
//   builtins:    cd pwd exit export unset set shift source . read history wait : true false
//   scripts:     sh file [args], sh -c 'cmd', source file ($0 $1.. $# work)
// there is no environment: variables live in the shell only.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAXTOK    160
#define MAXWORDS  31          // exec limit is 32 args
#define MAXSTAGES 8
#define MAXREDIR  4
#define MAXNODES  96
#define POOL      6000

// ---------------- variables ----------------
struct var { char name[24]; char val[128]; };
static struct var g_vars[40];
static int g_nvars;
static int g_status;                 // $?
static char g_argv0[64] = "sh";
static char* g_pos[10];              // $1..$9
static int g_npos;

static struct var* find_var(const char* n) {
    for (int i = 0; i < g_nvars; i++) if (strcmp(g_vars[i].name, n) == 0) return &g_vars[i];
    return 0;
}
static void set_var(const char* n, const char* v) {
    struct var* x = find_var(n);
    if (!x) {
        if (g_nvars >= 40 || strlen(n) >= 24) return;
        x = &g_vars[g_nvars++];
        strcpy(x->name, n);
    }
    strncpy(x->val, v, 127);
    x->val[127] = 0;
}
static void unset_var(const char* n) {
    for (int i = 0; i < g_nvars; i++)
        if (strcmp(g_vars[i].name, n) == 0) { g_vars[i] = g_vars[--g_nvars]; return; }
}
static void itoa_(int v, char* out) { sprintf(out, "%d", v); }

// value of $name into out
static void get_var(const char* n, char* out) {
    out[0] = 0;
    if (!strcmp(n, "?")) { itoa_(g_status, out); return; }
    if (!strcmp(n, "$")) { itoa_(getpid(), out); return; }
    if (!strcmp(n, "#")) { itoa_(g_npos, out); return; }
    if (!strcmp(n, "0")) { strcpy(out, g_argv0); return; }
    if (n[0] >= '1' && n[0] <= '9' && !n[1]) { int i = n[0] - '1'; if (i < g_npos) strcpy(out, g_pos[i]); return; }
    if (!strcmp(n, "PWD")) { getcwd(out, 128); return; }
    struct var* x = find_var(n);
    if (x) strcpy(out, x->val);
}

// ---------------- tokens ----------------
enum { T_WORD, T_PIPE, T_OR, T_AND, T_SEMI, T_BG, T_NL, T_LT, T_GT, T_GTGT, T_EGT, T_EGTGT, T_EDUP, T_ODUP, T_END };
struct tok { int type; const char* s; };
static struct tok g_tok[MAXTOK];
static int g_ntok;
static char g_pool[POOL];
static int g_pool_used;

// 0 ok, 1 incomplete (unterminated quote), -1 too big
static int tokenize(const char* t) {
    g_ntok = 0; g_pool_used = 0;
    int i = 0;
    for (;;) {
        while (t[i] == ' ' || t[i] == '\t') i++;
        if (!t[i]) break;
        if (g_ntok >= MAXTOK - 2) return -1;
        char c = t[i];
        if (c == '#') { while (t[i] && t[i] != '\n') i++; continue; }
        if (c == '\n') { g_tok[g_ntok++] = (struct tok){ T_NL, 0 }; i++; continue; }
        int ty = -1;
        if (c == '|') { if (t[i+1] == '|') { ty = T_OR; i += 2; } else { ty = T_PIPE; i++; } }
        else if (c == '&') { if (t[i+1] == '&') { ty = T_AND; i += 2; } else { ty = T_BG; i++; } }
        else if (c == ';') { ty = T_SEMI; i++; }
        else if (c == '<') { ty = T_LT; i++; }
        else if (c == '>') {
            if (t[i+1] == '>') { ty = T_GTGT; i += 2; }
            else if (t[i+1] == '&' && t[i+2] == '2') { ty = T_ODUP; i += 3; }
            else { ty = T_GT; i++; }
        } else if (c == '2' && t[i+1] == '>') {
            if (t[i+2] == '&' && t[i+3] == '1') { ty = T_EDUP; i += 4; }
            else if (t[i+2] == '>') { ty = T_EGTGT; i += 3; }
            else { ty = T_EGT; i += 2; }
        } else if (c == '1' && t[i+1] == '>' && t[i+2] == '&' && t[i+3] == '2') { ty = T_ODUP; i += 4; }
        if (ty >= 0) { g_tok[g_ntok++] = (struct tok){ ty, 0 }; continue; }

        // a word: runs to unquoted whitespace or operator; quotes are kept raw (expand() strips them)
        int s = i;
        char q = 0;
        while (t[i]) {
            char d = t[i];
            if (q) { if (d == q) q = 0; i++; continue; }
            if (d == '\'' || d == '"') { q = d; i++; continue; }
            if (d == '\\' && t[i+1]) { i += 2; continue; }
            if (d == ' ' || d == '\t' || d == '\n' || d == '|' || d == '&' || d == ';' || d == '<' || d == '>') break;
            i++;
        }
        if (q) return 1;
        int n = i - s;
        if (g_pool_used + n + 1 > POOL) return -1;
        char* w = g_pool + g_pool_used;
        memcpy(w, t + s, n); w[n] = 0;
        g_pool_used += n + 1;
        g_tok[g_ntok++] = (struct tok){ T_WORD, w };
    }
    g_tok[g_ntok++] = (struct tok){ T_END, 0 };
    return 0;
}

// ---------------- expansion ----------------
static int glob_match(const char* p, const char* s) {
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (p[1] == '*') p++;
            if (!p[1]) return 1;
            for (; *s; s++) if (glob_match(p + 1, s)) return 1;
            return glob_match(p + 1, s);
        }
        if (!*s) return 0;
        if (*p == '?') continue;
        if (*p == '[') {
            int neg = p[1] == '!' || p[1] == '^', ok = 0;
            const char* q = p + 1 + neg;
            for (; *q && (q == p + 1 + neg || *q != ']'); q++) {
                if (q[1] == '-' && q[2] && q[2] != ']') { if (*s >= q[0] && *s <= q[2]) ok = 1; q += 2; }
                else if (*q == *s) ok = 1;
            }
            if (!*q || ok == neg) return 0;
            p = q;
            continue;
        }
        if (*p == '\\' && p[1]) p++;
        if (*p != *s) return 0;
    }
    return !*s;
}

// expand one raw word into out; *glob set if it has unquoted * ? [ (those stay, quoted ones get a backslash)
static void expand(const char* w, char* out, int max, int* glob) {
    int o = 0, dq = 0;
    *glob = 0;
#define PUT(ch) do { if (o < max - 1) out[o++] = (ch); } while (0)
    if (w[0] == '~' && (w[1] == 0 || w[1] == '/')) { PUT('/'); w++; if (*w == '/') w++; }   // home is /
    while (*w) {
        char c = *w;
        if (c == '\'' && !dq) {
            for (w++; *w && *w != '\''; w++) { if (strchr("*?[\\", *w)) PUT('\\'); PUT(*w); }
            if (*w) w++;
        } else if (c == '"') { dq = !dq; w++; }
        else if (c == '\\' && w[1]) {
            if (dq && !strchr("$\"\\", w[1])) PUT('\\');
            else if (strchr("*?[\\", w[1])) PUT('\\');
            PUT(w[1]); w += 2;
        } else if (c == '$' && (w[1] == '{' || w[1] == '?' || w[1] == '$' || w[1] == '#' || (w[1] >= '0' && w[1] <= '9') || w[1] == '_' || (w[1] >= 'a' && w[1] <= 'z') || (w[1] >= 'A' && w[1] <= 'Z'))) {
            char name[32], val[160];
            int n = 0;
            w++;
            if (*w == '{') { w++; while (*w && *w != '}' && n < 31) name[n++] = *w++; if (*w) w++; }
            else if (strchr("?$#", *w) || (*w >= '0' && *w <= '9')) name[n++] = *w++;
            else while ((*w >= 'a' && *w <= 'z') || (*w >= 'A' && *w <= 'Z') || (*w >= '0' && *w <= '9') || *w == '_') { if (n < 31) name[n++] = *w; w++; }
            name[n] = 0;
            get_var(name, val);
            for (char* v = val; *v; v++) { if (strchr("*?[\\", *v)) PUT('\\'); PUT(*v); }
        } else {
            if (!dq && (c == '*' || c == '?' || c == '[')) *glob = 1;
            PUT(c); w++;
        }
    }
    out[o] = 0;
#undef PUT
}

static void unescape(char* s) {
    char* d = s;
    for (; *s; s++) { if (*s == '\\' && s[1]) s++; *d++ = *s; }
    *d = 0;
}

static char g_wordbuf[MAXWORDS + 2][130];
static int g_nwb;

static char* new_word(const char* s) {
    if (g_nwb >= MAXWORDS) return 0;
    strncpy(g_wordbuf[g_nwb], s, 129);
    g_wordbuf[g_nwb][129] = 0;
    return g_wordbuf[g_nwb++];
}

// expand a pattern with wildcards in its last component into sorted matches; returns how many added
static int do_glob(const char* pat, char** argv, int argc) {
    char dir[130], base[130];
    const char* slash = 0;
    for (const char* p = pat; *p; p++) if (*p == '/') slash = p;
    if (slash) { int l = slash - pat + 1; memcpy(dir, pat, l); dir[l] = 0; strcpy(base, slash + 1); }
    else { strcpy(dir, ""); strcpy(base, pat); }
    for (const char* p = dir; *p; p++) if (strchr("*?[", *p)) return 0;      // wildcards in directories: not supported
    char name[DIRENT_NAME_MAX];
    int start = argc;
    for (int i = 0; readdir_at(dir[0] ? dir : ".", i, name) == 1; i++) {
        if (name[0] == '.' && base[0] != '.') continue;
        if (!glob_match(base, name)) continue;
        char full[260];
        strcpy(full, dir); strcat(full, name);
        if (argc >= MAXWORDS) break;
        char* w = new_word(full);
        if (!w) break;
        int j = argc++;
        while (j > start && strcmp(argv[j - 1], w) > 0) { argv[j] = argv[j - 1]; j--; }
        argv[j] = w;
    }
    return argc - start;
}

// ---------------- parser (tokens -> nodes) ----------------
enum { N_SIMPLE, N_PIPE, N_AND, N_OR, N_SEQ, N_BG, N_IF, N_WHILE, N_FOR, N_NONE };
struct redir { int type; const char* target; };
struct node {
    int type;
    int a, b, c;                         // children (indexes); IF: cond, then, else; WHILE: cond, body; FOR: body
    int nw, nr, ns;                      // simple: words, redirs; pipe: stages
    const char* w[MAXWORDS];             // simple: raw words; FOR: w[0] = var, w[1..] = items
    struct redir r[MAXREDIR];
    int stage[MAXSTAGES];                // pipe
    int until;                           // WHILE: until loop
};
static struct node g_nodes[MAXNODES];
static int g_nnodes;
static int g_p;                          // parse position
static int g_perr;                       // 0 ok, 1 incomplete, 2 syntax error
static const char* g_errmsg;

static int new_node(int type) {
    if (g_nnodes >= MAXNODES) { g_perr = 2; g_errmsg = "command too long"; return 0; }
    struct node* n = &g_nodes[g_nnodes];
    memset(n, 0, sizeof(*n));
    n->type = type;
    n->a = n->b = n->c = -1;
    return g_nnodes++;
}

static int is_kw(const char* s) {
    static const char* k[] = { "if", "then", "elif", "else", "fi", "while", "until", "do", "done", "for", "in" };
    for (int i = 0; i < 11; i++) if (!strcmp(s, k[i])) return 1;
    return 0;
}
static int at_word(const char* kw) { return g_tok[g_p].type == T_WORD && !strcmp(g_tok[g_p].s, kw); }
static void skip_nl(void) { while (g_tok[g_p].type == T_NL || g_tok[g_p].type == T_SEMI) g_p++; }
static int fail(int incomplete, const char* msg) { if (!g_perr) { g_perr = incomplete ? 1 : 2; g_errmsg = msg; } return 0; }

static int parse_list(void);
static int parse_pipeline(void);

static int parse_command(void) {
    struct tok* t = &g_tok[g_p];
    if (t->type == T_END) return fail(1, "unexpected end");
    if (t->type == T_WORD && !strcmp(t->s, "if")) {
        g_p++;
        int n = new_node(N_IF);
        int c = parse_list(); if (g_perr) return 0;
        if (!at_word("then")) return fail(g_tok[g_p].type == T_END, "expected then");
        g_p++;
        int th = parse_list(); if (g_perr) return 0;
        int el = -1;
        if (at_word("elif")) {                       // elif chain: rewrite as nested if
            g_tok[g_p].s = "if";
            el = parse_command(); if (g_perr) return 0;
            g_nodes[n].a = c; g_nodes[n].b = th; g_nodes[n].c = el;
            return n;
        }
        if (at_word("else")) { g_p++; el = parse_list(); if (g_perr) return 0; }
        if (!at_word("fi")) return fail(g_tok[g_p].type == T_END, "expected fi");
        g_p++;
        g_nodes[n].a = c; g_nodes[n].b = th; g_nodes[n].c = el;
        return n;
    }
    if (t->type == T_WORD && (!strcmp(t->s, "while") || !strcmp(t->s, "until"))) {
        int until = t->s[0] == 'u';
        g_p++;
        int n = new_node(N_WHILE);
        int c = parse_list(); if (g_perr) return 0;
        if (!at_word("do")) return fail(g_tok[g_p].type == T_END, "expected do");
        g_p++;
        int body = parse_list(); if (g_perr) return 0;
        if (!at_word("done")) return fail(g_tok[g_p].type == T_END, "expected done");
        g_p++;
        g_nodes[n].a = c; g_nodes[n].b = body; g_nodes[n].until = until;
        return n;
    }
    if (t->type == T_WORD && !strcmp(t->s, "for")) {
        g_p++;
        int n = new_node(N_FOR);
        if (g_tok[g_p].type != T_WORD) return fail(g_tok[g_p].type == T_END, "for: name expected");
        g_nodes[n].w[0] = g_tok[g_p++].s;
        g_nodes[n].nw = 1;
        if (at_word("in")) {
            g_p++;
            while (g_tok[g_p].type == T_WORD && !at_word("do")) {
                if (g_nodes[n].nw >= MAXWORDS) return fail(0, "for: too many items");
                g_nodes[n].w[g_nodes[n].nw++] = g_tok[g_p++].s;
            }
        }
        skip_nl();
        if (!at_word("do")) return fail(g_tok[g_p].type == T_END, "expected do");
        g_p++;
        int body = parse_list(); if (g_perr) return 0;
        if (!at_word("done")) return fail(g_tok[g_p].type == T_END, "expected done");
        g_p++;
        g_nodes[n].a = body;
        return n;
    }

    int n = new_node(N_SIMPLE);
    if (g_perr) return 0;
    for (;;) {
        t = &g_tok[g_p];
        if (t->type == T_WORD) {
            if (g_nodes[n].nw == 0 && is_kw(t->s) && strcmp(t->s, "in")) break;       // then/fi/do/done end the list
            if (g_nodes[n].nw >= MAXWORDS) return fail(0, "too many words");
            g_nodes[n].w[g_nodes[n].nw++] = t->s;
            g_p++;
        } else if (t->type >= T_LT && t->type <= T_ODUP) {
            int ty = t->type;
            g_p++;
            const char* target = 0;
            if (ty != T_EDUP && ty != T_ODUP) {
                if (g_tok[g_p].type != T_WORD) return fail(g_tok[g_p].type == T_END, "missing redirection target");
                target = g_tok[g_p++].s;
            }
            if (g_nodes[n].nr >= MAXREDIR) return fail(0, "too many redirections");
            g_nodes[n].r[g_nodes[n].nr].type = ty;
            g_nodes[n].r[g_nodes[n].nr++].target = target;
        } else break;
    }
    if (g_nodes[n].nw == 0 && g_nodes[n].nr == 0) return fail(g_tok[g_p].type == T_END, "syntax error");
    return n;
}

static int parse_pipeline(void) {
    int first = parse_command(); if (g_perr) return 0;
    if (g_tok[g_p].type != T_PIPE) return first;
    int n = new_node(N_PIPE);
    g_nodes[n].stage[g_nodes[n].ns++] = first;
    while (g_tok[g_p].type == T_PIPE) {
        g_p++;
        while (g_tok[g_p].type == T_NL) g_p++;
        if (g_nodes[n].ns >= MAXSTAGES) return fail(0, "pipeline too long");
        int c = parse_command(); if (g_perr) return 0;
        g_nodes[n].stage[g_nodes[n].ns++] = c;
    }
    return n;
}

static int parse_andor(void) {
    int l = parse_pipeline(); if (g_perr) return 0;
    while (g_tok[g_p].type == T_AND || g_tok[g_p].type == T_OR) {
        int ty = g_tok[g_p].type == T_AND ? N_AND : N_OR;
        g_p++;
        while (g_tok[g_p].type == T_NL) g_p++;
        int r = parse_pipeline(); if (g_perr) return 0;
        int n = new_node(ty);
        g_nodes[n].a = l; g_nodes[n].b = r;
        l = n;
    }
    return l;
}

// list of and-or items separated by ; & newline, up to a closing keyword / end
static int parse_list(void) {
    int seq = -1;
    skip_nl();
    for (;;) {
        struct tok* t = &g_tok[g_p];
        if (t->type == T_END) { if (seq < 0) return fail(1, "unexpected end"); return seq; }
        if (t->type == T_WORD && is_kw(t->s) && strcmp(t->s, "if") && strcmp(t->s, "while") && strcmp(t->s, "until") && strcmp(t->s, "for"))
            { if (seq < 0) return fail(0, "syntax error near unexpected keyword"); return seq; }
        int item = parse_andor(); if (g_perr) return 0;
        if (g_tok[g_p].type == T_BG) { g_p++; int b = new_node(N_BG); g_nodes[b].a = item; item = b; }
        if (seq < 0) seq = item;
        else { int s = new_node(N_SEQ); g_nodes[s].a = seq; g_nodes[s].b = item; seq = s; }
        if (g_tok[g_p].type == T_SEMI || g_tok[g_p].type == T_NL) skip_nl();
        else if (g_tok[g_p].type != T_END && !(g_tok[g_p].type == T_WORD && is_kw(g_tok[g_p].s))) {
            if (g_tok[g_p - 1].type != T_BG) return fail(0, "syntax error");
        }
    }
}

// ---------------- execution ----------------
static int g_break, g_continue;          // pending break/continue levels
static int g_interactive;
static int g_nbg;
static int g_bgpids[16];

static int run_node(int ni);
static int run_script_text(const char* text);

static int expand_words(const char** raw, int nraw, char** argv) {
    int argc = 0;
    g_nwb = 0;
    char buf[300];
    for (int i = 0; i < nraw; i++) {
        int glob;
        expand(raw[i], buf, sizeof(buf), &glob);
        if (glob) {
            int got = do_glob(buf, argv, argc);
            if (got > 0) { argc += got; continue; }
        }
        unescape(buf);
        if (argc >= MAXWORDS) break;
        char* w = new_word(buf);
        if (!w) break;
        // an unquoted $var that expanded to nothing disappears (like sh)
        if (!buf[0] && !strchr(raw[i], '"') && !strchr(raw[i], '\'')) continue;
        argv[argc++] = w;
    }
    argv[argc] = 0;
    return argc;
}

static int apply_redirs(struct node* n) {
    char buf[300];
    int glob;
    for (int i = 0; i < n->nr; i++) {
        int ty = n->r[i].type;
        if (ty == T_EDUP) { dup2(1, 2); continue; }
        if (ty == T_ODUP) { dup2(2, 1); continue; }
        expand(n->r[i].target, buf, sizeof(buf), &glob);
        unescape(buf);
        int flags = ty == T_LT ? O_RDONLY : O_WRONLY | O_CREAT | ((ty == T_GTGT || ty == T_EGTGT) ? O_APPEND : O_TRUNC);
        int fd = open(buf, flags);
        if (fd < 0) { printf("sh: %s: cannot open\n", buf); return -1; }
        int target = ty == T_LT ? 0 : (ty == T_EGT || ty == T_EGTGT) ? 2 : 1;
        dup2(fd, target);
        close(fd);
    }
    return 0;
}

static void exec_path(char** argv) {
    char path[160];
    if (strchr(argv[0], '/')) {
        execv(argv[0], argv);
    } else {
        char pv[128];
        get_var("PATH", pv);
        const char* p = pv[0] ? pv : "/bin";
        while (*p) {
            int l = 0;
            while (p[l] && p[l] != ':') l++;
            memcpy(path, p, l); path[l] = 0;
            strcat(path, "/"); strcat(path, argv[0]);
            struct stat st;
            if (stat(path, &st) == 0 && !st.st_isdir) execv(path, argv);
            p += l; if (*p == ':') p++;
        }
    }
    printf("sh: %s: command not found\n", argv[0]);
    exit(127);
}

static int is_builtin(const char* s) {
    static const char* b[] = { "cd", "pwd", "exit", "export", "unset", "set", "shift", "source", ".", "read",
                               "history", "wait", ":", "true", "false", "break", "continue", 0 };
    for (int i = 0; b[i]; i++) if (!strcmp(s, b[i])) return 1;
    return 0;
}

static char g_hist[16][256];
static int g_nhist;

static int run_builtin(char** argv, int argc) {
    const char* c = argv[0];
    if (!strcmp(c, ":") || !strcmp(c, "true")) return 0;
    if (!strcmp(c, "false")) return 1;
    if (!strcmp(c, "cd")) {
        const char* d = argc > 1 ? argv[1] : "/";
        if (chdir(d) < 0) { printf("cd: %s: not a directory\n", d); return 1; }
        return 0;
    }
    if (!strcmp(c, "pwd")) { char cwd[128]; if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd); return 0; }
    if (!strcmp(c, "exit")) { fflush(NULL); exit(argc > 1 ? atoi(argv[1]) : g_status); }
    if (!strcmp(c, "export") || !strcmp(c, "set")) {
        if (argc == 1) { for (int i = 0; i < g_nvars; i++) printf("%s=%s\n", g_vars[i].name, g_vars[i].val); return 0; }
        for (int i = 1; i < argc; i++) {
            char* eq = strchr(argv[i], '=');
            if (eq) { *eq = 0; set_var(argv[i], eq + 1); }
        }
        return 0;
    }
    if (!strcmp(c, "unset")) { for (int i = 1; i < argc; i++) unset_var(argv[i]); return 0; }
    if (!strcmp(c, "shift")) {
        int n = argc > 1 ? atoi(argv[1]) : 1;
        if (n > g_npos) return 1;
        for (int i = 0; i + n < g_npos; i++) g_pos[i] = g_pos[i + n];
        g_npos -= n;
        return 0;
    }
    if (!strcmp(c, "read")) {
        char line[200];
        int n = 0, ch;
        while (n < 199 && read(0, &line[n], 1) == 1) { if (line[n] == '\n') break; n++; }
        ch = n;
        line[n] = 0;
        (void)ch;
        if (argc > 1) set_var(argv[1], line);
        return n == 0 && line[0] == 0 ? 1 : 0;
    }
    if (!strcmp(c, "history")) { for (int i = 0; i < g_nhist; i++) printf("%4d  %s\n", i + 1, g_hist[i]); return 0; }
    if (!strcmp(c, "wait")) { int st; while (waitpid(-1, &st, 0) > 0) {} g_nbg = 0; return 0; }
    if (!strcmp(c, "break")) { g_break = argc > 1 ? atoi(argv[1]) : 1; return 0; }
    if (!strcmp(c, "continue")) { g_continue = argc > 1 ? atoi(argv[1]) : 1; return 0; }
    if (!strcmp(c, "source") || !strcmp(c, ".")) {
        if (argc < 2) { printf("source: file name needed\n"); return 2; }
        struct stat st;
        FILE* f = stat(argv[1], &st) == 0 ? fopen(argv[1], "r") : 0;
        if (!f) { printf("source: %s: cannot open\n", argv[1]); return 1; }
        char* text = malloc(st.st_size + 2);
        if (!text) { fclose(f); return 1; }
        int n = fread(text, 1, st.st_size, f);
        text[n] = 0;
        fclose(f);
        int r = run_script_text(text);
        free(text);
        return r;
    }
    return 127;
}

static int wait_for(int pid) {
    int st = 0;
    while (waitpid(pid, &st, 0) < 0) {
        // interrupted: the child is still ours unless it is gone
        if (waitpid(pid, &st, WNOHANG) <= 0) return 130;
    }
    return WEXITSTATUS(st);
}

// run a SIMPLE node in the current process image (child): builtin or exec; never returns
static void child_simple(struct node* n) {
    char* argv[MAXWORDS + 1];
    int argc = expand_words(n->w, n->nw, argv);
    if (apply_redirs(n) < 0) exit(1);
    if (argc == 0) exit(0);
    if (is_builtin(argv[0])) { int r = run_builtin(argv, argc); fflush(NULL); exit(r); }
    exec_path(argv);
}

static int run_simple(struct node* n) {
    char* argv[MAXWORDS + 1];
    int argc = expand_words(n->w, n->nw, argv);

    // leading NAME=value words are assignments
    int k = 0;
    while (k < argc) {
        char* eq = strchr(argv[k], '=');
        if (!eq || eq == argv[k]) break;
        int ok = 1;
        for (char* p = argv[k]; p < eq; p++) if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || (p > argv[k] && *p >= '0' && *p <= '9'))) ok = 0;
        if (!ok) break;
        *eq = 0;
        set_var(argv[k], eq + 1);
        k++;
    }
    if (k == argc) return 0;
    char** av = argv + k;
    int ac = argc - k;

    if (is_builtin(av[0])) {
        int s0 = -1, s1 = -1, s2 = -1;
        if (n->nr) { s0 = dup(0); s1 = dup(1); s2 = dup(2); if (apply_redirs(n) < 0) { dup2(s0, 0); dup2(s1, 1); dup2(s2, 2); close(s0); close(s1); close(s2); return 1; } }
        int r = run_builtin(av, ac);
        fflush(NULL);
        if (n->nr) { dup2(s0, 0); dup2(s1, 1); dup2(s2, 2); close(s0); close(s1); close(s2); }
        return r;
    }
    int pid = fork();
    if (pid < 0) { printf("sh: fork failed\n"); return 1; }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        if (apply_redirs(n) < 0) exit(1);
        exec_path(av);
    }
    return wait_for(pid);
}

static int run_pipe(struct node* n) {
    int pids[MAXSTAGES], prev = -1, p[2], np = 0;
    for (int i = 0; i < n->ns; i++) {
        int last = i == n->ns - 1;
        if (!last && pipe(p) < 0) { printf("sh: pipe failed\n"); break; }
        int pid = fork();
        if (pid < 0) { printf("sh: fork failed\n"); break; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (prev >= 0) { dup2(prev, 0); close(prev); }
            if (!last) { dup2(p[1], 1); close(p[0]); close(p[1]); }
            struct node* c = &g_nodes[n->stage[i]];
            if (c->type == N_SIMPLE) child_simple(c);
            int r = run_node(n->stage[i]);
            fflush(NULL);
            exit(r);
        }
        pids[np++] = pid;
        if (prev >= 0) close(prev);
        if (!last) { close(p[1]); prev = p[0]; }
    }
    int st = 0;
    for (int i = 0; i < np; i++) st = wait_for(pids[i]);
    return st;
}

static int run_node(int ni) {
    struct node* n = &g_nodes[ni];
    switch (n->type) {
    case N_SIMPLE: return g_status = run_simple(n);
    case N_PIPE:   return g_status = run_pipe(n);
    case N_AND: { int r = run_node(n->a); if (r == 0 && !g_break && !g_continue) r = run_node(n->b); return g_status = r; }
    case N_OR:  { int r = run_node(n->a); if (r != 0 && !g_break && !g_continue) r = run_node(n->b); return g_status = r; }
    case N_SEQ: { run_node(n->a); if (g_break || g_continue) return g_status; return run_node(n->b); }
    case N_BG: {
        fflush(NULL);
        int pid = fork();
        if (pid == 0) { signal(SIGINT, SIG_DFL); int r = run_node(n->a); fflush(NULL); exit(r); }
        if (pid > 0) { if (g_nbg < 16) g_bgpids[g_nbg++] = pid; if (g_interactive) printf("[%d]\n", pid); }
        return g_status = 0;
    }
    case N_IF: {
        int r = run_node(n->a);
        if (g_break || g_continue) return r;
        if (r == 0) return run_node(n->b);
        if (n->c >= 0) return run_node(n->c);
        return g_status = 0;
    }
    case N_WHILE: {
        int r = 0;
        for (;;) {
            int c = run_node(n->a);
            if (g_break || g_continue) break;
            if ((c == 0) == n->until) break;
            r = run_node(n->b);
            if (g_break) { g_break--; break; }
            if (g_continue) { if (--g_continue) break; }
        }
        return g_status = r;
    }
    case N_FOR: {
        char* items[MAXWORDS + 1];
        int ni2 = expand_words(n->w + 1, n->nw - 1, items);
        char saved[MAXWORDS][130];
        for (int i = 0; i < ni2; i++) { strncpy(saved[i], items[i], 129); saved[i][129] = 0; }
        int r = 0;
        for (int i = 0; i < ni2; i++) {
            set_var(n->w[0], saved[i]);
            r = run_node(n->a);
            if (g_break) { g_break--; break; }
            if (g_continue) { if (--g_continue) break; }
        }
        return g_status = r;
    }
    }
    return 0;
}

// parse + run a whole text (script or one interactive command). returns status; -1 incomplete (interactive only)
static int run_text(const char* text, int* incomplete) {
    int t = tokenize(text);
    if (incomplete) *incomplete = 0;
    if (t == 1) { if (incomplete) { *incomplete = 1; return 0; } printf("sh: unterminated quote\n"); return 2; }
    if (t < 0) { printf("sh: command too long\n"); return 2; }
    g_nnodes = 0; g_p = 0; g_perr = 0;
    int root = -1;
    skip_nl();
    if (g_tok[g_p].type != T_END) root = parse_list();
    if (!g_perr && g_tok[g_p].type != T_END) { g_perr = 2; g_errmsg = "syntax error near unexpected token"; }
    if (g_perr == 1 && incomplete) { *incomplete = 1; return 0; }
    if (g_perr) { printf("sh: %s\n", g_errmsg ? g_errmsg : "syntax error"); return g_status = 2; }
    if (root < 0) return 0;
    return run_node(root);
}

// source/sh-script from inside a running command: keep the caller's parse (tokens, nodes, word pool) intact
static int run_script_text(const char* text) {
    size_t sz = sizeof(g_tok) + sizeof(g_pool) + sizeof(g_nodes);
    char* save = malloc(sz);
    if (!save) { printf("sh: out of memory\n"); return 1; }
    int ints[5] = { g_ntok, g_pool_used, g_nnodes, g_p, g_perr };
    memcpy(save, g_tok, sizeof(g_tok));
    memcpy(save + sizeof(g_tok), g_pool, sizeof(g_pool));
    memcpy(save + sizeof(g_tok) + sizeof(g_pool), g_nodes, sizeof(g_nodes));
    int inc;
    int r = run_text(text, &inc);
    if (inc) { printf("sh: unexpected end of file\n"); r = 2; }
    memcpy(g_tok, save, sizeof(g_tok));
    memcpy(g_pool, save + sizeof(g_tok), sizeof(g_pool));
    memcpy(g_nodes, save + sizeof(g_tok) + sizeof(g_pool), sizeof(g_nodes));
    g_ntok = ints[0]; g_pool_used = ints[1]; g_nnodes = ints[2]; g_p = ints[3]; g_perr = ints[4];
    free(save);
    return r;
}

// ---- line editor (raw console; '\b' moves the VGA cursor left without erasing) ----
#define LINEMAX 256
#define HIST    16
#define NCAND   64
#define CANDLEN 48

static char g_line[LINEMAX];
static int  g_len, g_cur;
static char g_cand[NCAND][CANDLEN];   // completion candidates (dirs get a trailing '/')
static int  g_ncand;

static void out(const char* s, int n) { write(1, s, n); }
static void rep(char c, int n) { while (n-- > 0) out(&c, 1); }
static int g_ps2;      // 1: continuation prompt ("> ")
static void prompt(void) {
    char cwd[128];
    if (g_ps2) { out("> ", 2); return; }
    printf("%s $ ", getcwd(cwd, sizeof(cwd)) ? cwd : "?");
}

// rewrite line[from..], blank 'extra' cells after it, put the cursor back at g_cur
static void redraw_from(int from, int extra) {
    out(g_line + from, g_len - from);
    rep(' ', extra);
    rep('\b', g_len - g_cur + extra);
}

static void move_to(int pos) {
    if (pos < g_cur) rep('\b', g_cur - pos);
    else out(g_line + g_cur, pos - g_cur);
    g_cur = pos;
}

static void insert(const char* s, int n) {
    if (n > LINEMAX - 1 - g_len) n = LINEMAX - 1 - g_len;
    if (n <= 0) return;
    memmove(g_line + g_cur + n, g_line + g_cur, g_len - g_cur);
    memcpy(g_line + g_cur, s, n);
    g_len += n;
    int at = g_cur;
    g_cur += n;
    out(g_line + at, n);
    if (g_cur < g_len) redraw_from(g_cur, 0);
}

static void erase(int from, int to) {   // delete line[from..to), cursor ends at from
    move_to(from);
    memmove(g_line + from, g_line + to, g_len - to);
    g_len -= to - from;
    redraw_from(from, to - from);
}

static void set_line(const char* s) {   // replace the whole line (history)
    int old = g_len;
    move_to(0);
    g_len = strlen(s);
    memcpy(g_line, s, g_len);
    g_cur = g_len;
    out(g_line, g_len);
    if (old > g_len) { rep(' ', old - g_len); rep('\b', old - g_len); }
}

static void add_cand(const char* name, int isdir, const char* base) {
    int bl = strlen(base), nl = strlen(name);
    if (g_ncand >= NCAND || nl + 1 >= CANDLEN || strncmp(name, base, bl) != 0) return;
    strcpy(g_cand[g_ncand], name);
    if (isdir) strcpy(g_cand[g_ncand] + nl, "/");
    g_ncand++;
}

// collect candidates for the word line[start..cur); returns the length of the typed basename
static int collect(int start) {
    char word[LINEMAX], dir[LINEMAX];
    int wl = g_cur - start;
    memcpy(word, g_line + start, wl);
    word[wl] = 0;
    g_ncand = 0;

    int cmdpos = 1;                              // first word, or right after a '|'
    for (int i = start - 1; i >= 0; i--) {
        if (g_line[i] == ' ') continue;
        cmdpos = g_line[i] == '|' || g_line[i] == ';' || g_line[i] == '&';
        break;
    }
    char* slash = 0;
    for (char* q = word; *q; q++) if (*q == '/') slash = q;
    if (cmdpos && !slash) {
        const char* b[] = { "cd", "pwd", "exit", "export", "unset", "source", "read", "shift", "history" };
        for (int i = 0; i < 9; i++) add_cand(b[i], 0, word);
        strcpy(dir, "/bin");
    } else if (slash) {
        int dl = slash - word + 1;
        memcpy(dir, word, dl);
        dir[dl] = 0;                             // "" stays "", "/" stays "/"
    } else {
        strcpy(dir, ".");
    }
    const char* base = slash ? slash + 1 : word;

    char name[DIRENT_NAME_MAX], full[LINEMAX + DIRENT_NAME_MAX];
    for (int i = 0; readdir_at(dir, i, name) == 1; i++) {
        struct stat st;
        int isdir = 0;
        if (name[0] == '.' && base[0] != '.') continue;
        if (!(cmdpos && !slash)) {
            strcpy(full, dir);
            if (dir[strlen(dir) - 1] != '/') strcat(full, "/");
            strcat(full, name);
            isdir = stat(full, &st) == 0 && st.st_isdir;
        }
        add_cand(name, isdir, base);
    }
    return strlen(base);
}

static void list_cands(void) {
    int w = 0, col = 0;
    for (int i = 0; i < g_ncand; i++) if ((int)strlen(g_cand[i]) > w) w = strlen(g_cand[i]);
    w += 2;
    int per = 80 / w;
    out("\n", 1);
    for (int i = 0; i < g_ncand; i++) {
        int l = strlen(g_cand[i]);
        out(g_cand[i], l);
        if (++col == per || i == g_ncand - 1) { out("\n", 1); col = 0; }
        else rep(' ', w - l);
    }
    prompt();
    out(g_line, g_len);
    rep('\b', g_len - g_cur);
}

static void complete(int second) {
    int start = g_cur;
    while (start > 0 && g_line[start - 1] != ' ') start--;
    int bl = collect(start);
    if (g_ncand == 0) return;
    int lcp = strlen(g_cand[0]);                 // longest common prefix of the candidates
    for (int i = 1; i < g_ncand; i++) {
        int k = 0;
        while (k < lcp && g_cand[i][k] == g_cand[0][k]) k++;
        lcp = k;
    }
    if (g_ncand == 1) {
        int l = strlen(g_cand[0]);
        insert(g_cand[0] + bl, l - bl);
        if (g_cand[0][l - 1] != '/') insert(" ", 1);
    } else if (lcp > bl) {
        insert(g_cand[0] + bl, lcp - bl);
    } else if (second) {
        list_cands();
    }
}

// read one line in raw mode into g_line; returns 0 on ctrl-d at an empty line
static int readline(void) {
    unsigned char c;
    int browse = g_nhist, last_tab = 0;
    char saved[LINEMAX] = "";
    g_len = g_cur = 0;
    g_line[0] = 0;
    tcsetraw(1);
    for (;;) {
        if (read(0, &c, 1) != 1) { tcsetraw(0); return 0; }
        int tab = 0;
        if (c == '\n') break;
        if (c == '\t') { complete(last_tab); tab = 1; }
        else if (c == 3) { move_to(g_len); out("^C\n", 3); g_len = g_cur = 0; prompt(); }
        else if (c == 4) { if (g_len == 0) { out("\n", 1); tcsetraw(0); return 0; } }
        else if (c == 12) { out("\033[2J\033[H", 7); prompt(); out(g_line, g_len); rep('\b', g_len - g_cur); }
        else if (c == 1) move_to(0);
        else if (c == 5) move_to(g_len);
        else if (c == 21) erase(0, g_cur);                    // kill text before the cursor
        else if (c == 127 || c == 8) { if (g_cur > 0) erase(g_cur - 1, g_cur); }
        else if (c == 27) {
            unsigned char a = 0, b = 0;
            read(0, &a, 1);
            if (a != '[') continue;
            read(0, &b, 1);
            if (b == 'C') { if (g_cur < g_len) move_to(g_cur + 1); }
            else if (b == 'D') { if (g_cur > 0) move_to(g_cur - 1); }
            else if (b == 'H') move_to(0);
            else if (b == 'F') move_to(g_len);
            else if (b == '3') { read(0, &a, 1); if (g_cur < g_len) erase(g_cur, g_cur + 1); }
            else if (b == 'A' && browse > 0) {
                if (browse == g_nhist) { memcpy(saved, g_line, g_len); saved[g_len] = 0; }
                set_line(g_hist[--browse]);
            } else if (b == 'B' && browse < g_nhist) {
                browse++;
                set_line(browse == g_nhist ? saved : g_hist[browse]);
            }
        }
        else if (c >= 32 && c < 127) { char ch = c; insert(&ch, 1); }
        last_tab = tab;
    }
    out("\n", 1);
    tcsetraw(0);
    g_line[g_len] = 0;
    if (g_len > 0 && (g_nhist == 0 || strcmp(g_hist[g_nhist - 1], g_line) != 0)) {
        if (g_nhist == HIST) { memmove(g_hist[0], g_hist[1], (HIST - 1) * LINEMAX); g_nhist--; }
        strcpy(g_hist[g_nhist++], g_line);
    }
    return 1;
}


static char* read_file(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0 || st.st_isdir) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char* text = malloc(st.st_size + 2);
    if (!text) { fclose(f); return 0; }
    int n = fread(text, 1, st.st_size, f);
    text[n] = 0;
    fclose(f);
    return text;
}

int main(int argc, char** argv) {
    set_var("PATH", "/bin");
    set_var("HOME", "/");
    set_var("USER", "root");

    // non-interactive: sh -c 'cmd' [name args..]  /  sh script [args..]
    if (argc >= 2) {
        char* text;
        int first;                         // first positional parameter in argv
        if (!strcmp(argv[1], "-c") && argc >= 3) {
            text = argv[2];
            if (argc > 3) strncpy(g_argv0, argv[3], 63);
            first = 4;
        } else {
            text = read_file(argv[1]);
            if (!text) { printf("sh: %s: cannot open\n", argv[1]); return 127; }
            strncpy(g_argv0, argv[1], 63);
            first = 2;
        }
        for (int i = first; i < argc && g_npos < 9; i++) g_pos[g_npos++] = argv[i];
        int inc;
        int r = run_text(text, &inc);
        if (inc) { printf("sh: unexpected end of input\n"); r = 2; }
        fflush(NULL);
        return r;
    }

    g_interactive = 1;
    signal(SIGINT, SIG_IGN);
    printf("bobliu sh (pid %d). commands live in /bin; 'exit' leaves.\n", getpid());
    static char acc[2048];
    acc[0] = 0;
    for (;;) {
        int st;
        while (waitpid(-1, &st, WNOHANG) > 0) {}      // reap finished background jobs
        g_nbg = 0;
        g_ps2 = acc[0] != 0;
        prompt();
        if (!readline()) break;
        if (strlen(acc) + strlen(g_line) + 2 >= sizeof(acc)) { printf("sh: command too long\n"); acc[0] = 0; continue; }
        if (acc[0]) strcat(acc, "\n");
        strcat(acc, g_line);
        int inc;
        int r = run_text(acc, &inc);
        if (inc) continue;
        acc[0] = 0;
        g_status = r;
    }
    return 0;
}
