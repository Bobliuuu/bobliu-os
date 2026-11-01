// bobliu user-space shell: reads a line, fork+exec's it, waits.
// raw-mode line editor with tab completion and history.
// supports cd/pwd/exit, "cmd | cmd | ...", and "> file" / ">> file" on the last command
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAXARGS   16
#define MAXSTAGES 6

static char* g_argv[MAXSTAGES][MAXARGS + 1];

static void exec_cmd(char** argv) {
    char path[128];
    if (strchr(argv[0], '/')) {
        strcpy(path, argv[0]);
    } else {
        strcpy(path, "/bin/");
        strcpy(path + 5, argv[0]);
    }
    execv(path, argv);
    printf("sh: %s: not found\n", argv[0]);
    exit(127);
}

static void run(int nstages, const char* redir, int append) {
    int pids[MAXSTAGES];
    int prev = -1, p[2];

    for (int i = 0; i < nstages; i++) {
        int last = i == nstages - 1;
        if (!last && pipe(p) < 0) { printf("sh: pipe failed\n"); break; }

        int pid = fork();
        if (pid < 0) { printf("sh: fork failed\n"); break; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);                 // ctrl-c should kill what we run, not the shell
            if (prev >= 0) { dup2(prev, 0); close(prev); }
            if (!last) { dup2(p[1], 1); close(p[0]); close(p[1]); }
            if (last && redir) {
                int fd = open(redir, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC));
                if (fd < 0) { printf("sh: cannot open %s\n", redir); exit(1); }
                dup2(fd, 1);
                close(fd);
            }
            exec_cmd(g_argv[i]);
        }
        pids[i] = pid;
        if (prev >= 0) close(prev);
        if (!last) { close(p[1]); prev = p[0]; }
    }

    int status = 0;
    for (int i = 0; i < nstages; i++) {
        if (pids[i] > 0) waitpid(pids[i], &status, 0);
    }
    if (WEXITSTATUS(status) != 0) printf("[exit %d]\n", WEXITSTATUS(status));
}

// ---- line editor (raw console; '\b' moves the VGA cursor left without erasing) ----
#define LINEMAX 128
#define HIST    16
#define NCAND   64
#define CANDLEN 48

static char g_line[LINEMAX];
static int  g_len, g_cur;
static char g_hist[HIST][LINEMAX];
static int  g_nhist;
static char g_cand[NCAND][CANDLEN];   // completion candidates (dirs get a trailing '/')
static int  g_ncand;

static void out(const char* s, int n) { write(1, s, n); }
static void rep(char c, int n) { while (n-- > 0) out(&c, 1); }
static void prompt(void) {
    char cwd[128];
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
        cmdpos = g_line[i] == '|';
        break;
    }
    char* slash = 0;
    for (char* q = word; *q; q++) if (*q == '/') slash = q;
    if (cmdpos && !slash) {
        const char* b[] = { "cd", "pwd", "exit" };
        for (int i = 0; i < 3; i++) add_cand(b[i], 0, word);
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

int main(void) {
    char line[128];

    signal(SIGINT, SIG_IGN);
    printf("bobliu sh (pid %d). commands live in /bin; 'exit' leaves.\n", getpid());
    for (;;) {
        prompt();
        if (!readline()) break;
        strcpy(line, g_line);

        int stage = 0, argc = 0;
        const char* redir = 0;
        int append = 0, bad = 0;
        char* p = line;
        while (*p) {
            while (*p == ' ') *p++ = '\0';
            if (!*p) break;
            char* tok = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';                    // terminate the word

            if (strcmp(tok, "|") == 0) {
                if (argc == 0 || stage + 1 >= MAXSTAGES) { bad = 1; break; }
                g_argv[stage][argc] = 0;
                stage++;
                argc = 0;
                continue;
            }
            if (*tok == '>') {
                append = tok[1] == '>';
                tok += append ? 2 : 1;
                if (!*tok) {                        // "> file": the name is the next word
                    while (*p == ' ') p++;
                    tok = p;
                    while (*p && *p != ' ') p++;
                    if (*p) *p++ = '\0';
                }
                if (!*tok) { bad = 1; break; }
                redir = tok;
                continue;
            }
            if (argc >= MAXARGS) { bad = 1; break; }
            g_argv[stage][argc++] = tok;
        }
        g_argv[stage][argc] = 0;
        if (bad || (stage > 0 && argc == 0)) { printf("sh: syntax error\n"); continue; }
        if (stage == 0 && argc == 0) continue;

        char** argv = g_argv[0];
        if (stage == 0) {
            if (strcmp(argv[0], "exit") == 0) break;
            if (strcmp(argv[0], "cd") == 0) {
                if (chdir(argc > 1 ? argv[1] : "/") < 0) printf("cd: %s: not a directory\n", argc > 1 ? argv[1] : "/");
                continue;
            }
            if (strcmp(argv[0], "pwd") == 0) {
                char cwd[128];
                if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
                continue;
            }
        }
        run(stage + 1, redir, append);
    }
    return 0;
}
