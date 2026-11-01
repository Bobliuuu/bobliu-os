// lite vi: NORMAL / INSERT / ':' modes on the raw VGA console (ANSI subset, see docs)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define ROWS 24          // text rows; row 25 is the status line
#define COLS 79          // never touch the last cell (would scroll)
#define K_UP 256
#define K_DOWN 257
#define K_RIGHT 258
#define K_LEFT 259
#define K_HOME 260
#define K_END 261
#define K_DEL 262

enum { NORMAL, INSERT, CMD };

static char** L;         // lines, each malloc'd NUL-terminated
static int nl, cy, cx, top, left, mode, dirty;
static char fname[128], msg[80], cmd[80];
static int cmdlen;
static char** U;         // undo snapshot (one level)
static int unl, ucy, ucx, udirty, have_undo;

static char* sdup(const char* s) { char* p = malloc(strlen(s) + 1); strcpy(p, s); return p; }

static void ins_line(int at, const char* s) {
    L = realloc(L, sizeof(char*) * (nl + 1));
    for (int i = nl; i > at; i--) L[i] = L[i - 1];
    L[at] = sdup(s);
    nl++;
}

static void del_line(int at) {
    free(L[at]);
    for (int i = at; i < nl - 1; i++) L[i] = L[i + 1];
    nl--;
    if (nl == 0) ins_line(0, "");
}

static void snap(void) {
    for (int i = 0; i < unl; i++) free(U[i]);
    free(U);
    U = malloc(sizeof(char*) * nl);
    for (int i = 0; i < nl; i++) U[i] = sdup(L[i]);
    unl = nl; ucy = cy; ucx = cx; udirty = dirty; have_undo = 1;
}

static void undo(void) {
    if (!have_undo) { strcpy(msg, "nothing to undo"); return; }
    for (int i = 0; i < nl; i++) free(L[i]);
    free(L);
    L = U; nl = unl; U = 0; unl = 0; have_undo = 0;
    cy = ucy; cx = ucx; dirty = udirty;
}

static void load(const char* path) {
    int fd = open(path, O_RDONLY);
    nl = 0;
    if (fd < 0) { ins_line(0, ""); strcpy(msg, "[new file]"); return; }
    char* b = 0; int n = 0, cap = 0, r;
    char tmp[256];
    while ((r = read(fd, tmp, sizeof tmp)) > 0) {
        if (n + r + 1 > cap) { cap = (n + r + 1) * 2; b = realloc(b, cap); }
        memcpy(b + n, tmp, r); n += r;
    }
    close(fd);
    int s = 0;
    for (int i = 0; i < n; i++) {
        if (b[i] == '\n') { b[i] = 0; ins_line(nl, b + s); s = i + 1; }
    }
    if (s < n) { b[n] = 0; ins_line(nl, b + s); }
    if (nl == 0) ins_line(0, "");
    free(b);
    snprintf(msg, sizeof msg, "\"%s\" %d lines", path, nl);
}

static int save(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { snprintf(msg, sizeof msg, "cannot write %s", path); return -1; }
    int bytes = 0;
    for (int i = 0; i < nl; i++) {
        int l = strlen(L[i]);
        if (write(fd, L[i], l) != l || write(fd, "\n", 1) != 1) { close(fd); strcpy(msg, "write error"); return -1; }
        bytes += l + 1;
    }
    close(fd);
    snprintf(msg, sizeof msg, "\"%s\" %d lines, %d bytes written", path, nl, bytes);
    return 0;
}

static int maxx(void) {  // last valid cursor column
    int l = strlen(L[cy]);
    return mode == INSERT ? l : (l ? l - 1 : 0);
}

static void clamp(void) {
    if (cy < 0) cy = 0;
    if (cy >= nl) cy = nl - 1;
    if (cx > maxx()) cx = maxx();
    if (cx < 0) cx = 0;
}

static char scr[ROWS * (COLS + 16) + 256];

static void draw(void) {
    clamp();
    if (cy < top) top = cy;
    if (cy >= top + ROWS) top = cy - ROWS + 1;
    if (cx < left) left = cx;
    if (cx >= left + COLS) left = cx - COLS + 1;
    int o = 0;
    for (int r = 0; r < ROWS; r++) {
        o += snprintf(scr + o, 16, "\033[%d;1H", r + 1);
        int li = top + r;
        if (li < nl) {
            int l = strlen(L[li]);
            for (int c = left; c < l && c < left + COLS; c++) {
                unsigned char ch = L[li][c];
                scr[o++] = (ch >= 32 && ch < 127) ? ch : ' ';
            }
        } else scr[o++] = '~';
        o += snprintf(scr + o, 8, "\033[K");
    }
    write(1, scr, o);

    char st[200];
    static const char* mn[] = { "NORMAL", "INSERT", "COMMAND" };
    char l[100];
    if (mode == CMD) snprintf(l, sizeof l, ":%s", cmd);
    else snprintf(l, sizeof l, " %s  %s%s  %s", mn[mode], fname, dirty ? " [modified]" : "", msg);
    char rt[24];
    snprintf(rt, sizeof rt, "%d:%d ", cy + 1, cx + 1);
    int w = COLS;
    memset(st, ' ', w);
    memcpy(st, l, strlen(l) < (unsigned)(w - strlen(rt)) ? strlen(l) : w - strlen(rt));
    memcpy(st + w - strlen(rt), rt, strlen(rt));
    if (mode == CMD) {          // plain command line, no right-hand position
        memset(st, ' ', w);
        memcpy(st, l, strlen(l) < (unsigned)w ? strlen(l) : w);
    }
    st[w] = 0;
    char out[256];
    int n = snprintf(out, sizeof out, "\033[25;1H%s%s\033[0m", mode == CMD ? "" : "\033[7m", st);
    if (mode == CMD) n += snprintf(out + n, 20, "\033[25;%dH", (int)strlen(l) + 1);
    else n += snprintf(out + n, 20, "\033[%d;%dH", cy - top + 1, cx - left + 1);
    write(1, out, n);
}

static unsigned char ib[32];
static int ilen, ipos;

static int getkey(void) {
    if (ipos >= ilen) {
        ilen = read(0, ib, sizeof ib);
        ipos = 0;
        if (ilen <= 0) { ilen = 0; return -1; }
    }
    int c = ib[ipos++];
    if (c == 27 && ipos < ilen && ib[ipos] == '[') {
        ipos++;
        int d = ipos < ilen ? ib[ipos++] : 0;
        switch (d) {
        case 'A': return K_UP;    case 'B': return K_DOWN;
        case 'C': return K_RIGHT; case 'D': return K_LEFT;
        case 'H': return K_HOME;  case 'F': return K_END;
        case '3': if (ipos < ilen) ipos++; return K_DEL;
        }
        return 0;
    }
    return c;
}

static void move(int k) {
    switch (k) {
    case 'h': case K_LEFT:  if (cx > 0) cx--; break;
    case 'l': case K_RIGHT: if (cx < maxx()) cx++; break;
    case 'j': case K_DOWN:  if (cy < nl - 1) cy++; break;
    case 'k': case K_UP:    if (cy > 0) cy--; break;
    case '0': case K_HOME:  cx = 0; break;
    case '$': case K_END:   cx = 1 << 20; break;
    }
    clamp();
}

static void insch(int c) {
    int l = strlen(L[cy]);
    L[cy] = realloc(L[cy], l + 2);
    memmove(L[cy] + cx + 1, L[cy] + cx, l - cx + 1);
    L[cy][cx++] = c;
    dirty = 1;
}

static void delch(void) {   // delete char under cursor
    int l = strlen(L[cy]);
    if (cx >= l) return;
    memmove(L[cy] + cx, L[cy] + cx + 1, l - cx);
    dirty = 1;
}

static void insert_key(int k) {
    if (k == 27) { mode = NORMAL; if (cx > 0) cx--; return; }
    if (k == '\n') {
        char* rest = sdup(L[cy] + cx);
        L[cy][cx] = 0;
        ins_line(cy + 1, rest);
        free(rest);
        cy++; cx = 0; dirty = 1;
    } else if (k == 127 || k == 8) {
        if (cx > 0) { cx--; delch(); }
        else if (cy > 0) {
            int pl = strlen(L[cy - 1]);
            L[cy - 1] = realloc(L[cy - 1], pl + strlen(L[cy]) + 1);
            strcat(L[cy - 1], L[cy]);
            del_line(cy);
            cy--; cx = pl; dirty = 1;
        }
    } else if (k == K_DEL) delch();
    else if (k == '\t') { for (int i = 0; i < 4; i++) insch(' '); }
    else if (k >= 32 && k < 127) insch(k);
    else if (k >= K_UP && k <= K_END) move(k);
}

static int do_cmd(void) {   // returns 1 to quit
    char* c = cmd;
    while (*c == ' ') c++;
    char* arg = c;
    while (*arg && *arg != ' ') arg++;
    char w[16]; int wl = arg - c;
    if (wl > 15) wl = 15;
    memcpy(w, c, wl); w[wl] = 0;
    while (*arg == ' ') arg++;
    int bang = wl && w[wl - 1] == '!';
    if (bang) w[wl - 1] = 0;
    const char* target = *arg ? arg : fname;
    if (!strcmp(w, "w") || !strcmp(w, "wq") || !strcmp(w, "x")) {
        if (!*target) { strcpy(msg, "no file name"); return 0; }
        if (save(target) < 0) return 0;
        if (target == fname || !fname[0]) { dirty = 0; if (!fname[0]) strncpy(fname, target, 127); }
        return w[0] != 'w' || w[1] == 'q';
    }
    if (!strcmp(w, "q")) {
        if (dirty && !bang) { strcpy(msg, "No write since last change (use :q!)"); return 0; }
        return 1;
    }
    snprintf(msg, sizeof msg, "unknown command: %s", w);
    return 0;
}

static void normal_key(int k) {
    static int pend;
    msg[0] = 0;
    if (pend) {
        int p = pend; pend = 0;
        if (p == 'g' && k == 'g') { cy = 0; clamp(); }
        else if (p == 'd' && k == 'd') {
            snap(); del_line(cy); dirty = 1; clamp();
        }
        return;
    }
    switch (k) {
    case 'g': case 'd': pend = k; break;
    case 'G': cy = nl - 1; clamp(); break;
    case 'x': case K_DEL: snap(); delch(); clamp(); break;
    case 'u': undo(); break;
    case 'i': snap(); mode = INSERT; break;
    case 'a': snap(); mode = INSERT; if (L[cy][0]) cx++; break;
    case 'A': snap(); mode = INSERT; cx = strlen(L[cy]); break;
    case 'I': snap(); mode = INSERT; cx = 0; break;
    case 'o': snap(); ins_line(cy + 1, ""); cy++; cx = 0; mode = INSERT; dirty = 1; break;
    case 'O': snap(); ins_line(cy, ""); cx = 0; mode = INSERT; dirty = 1; break;
    case ':': mode = CMD; cmdlen = 0; cmd[0] = 0; break;
    default: move(k);
    }
}

int main(int argc, char** argv) {
    if (argc > 1) strncpy(fname, argv[1], sizeof fname - 1);
    if (fname[0]) load(fname); else ins_line(0, "");
    tcsetraw(1);
    write(1, "\033[2J", 4);
    for (;;) {
        draw();
        int k = getkey();
        if (k < 0) break;
        if (mode == INSERT) insert_key(k);
        else if (mode == CMD) {
            if (k == 27) mode = NORMAL;
            else if (k == '\n') { mode = NORMAL; msg[0] = 0; if (do_cmd()) break; }
            else if (k == 127 || k == 8) { if (cmdlen) cmd[--cmdlen] = 0; else mode = NORMAL; }
            else if (k >= 32 && k < 127 && cmdlen < 70) { cmd[cmdlen++] = k; cmd[cmdlen] = 0; }
        } else normal_key(k);
    }
    tcsetraw(0);
    write(1, "\033[2J", 4);
    return 0;
}
