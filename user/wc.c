// wc [-lwcm] [file...]: lines, words, bytes; several files get a total line
#include "common.h"
struct cnt { int l, w, c; };
static int fl, fw, fc, width;

static void show(const struct cnt* t, const char* name) {
    int first = 1;
    #define N(f, v) if (f) { outf("%*d", first ? width : width + 1, v); first = 0; }
    N(fl, t->l) N(fw, t->w) N(fc, t->c)
    if (name) outf(" %s", name);
    oc('\n');
}
static int digits(int v) { int n = 1; while (v >= 10) { v /= 10; n++; } return n; }

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char* p = argv[i] + 1; *p; p++) {
            if (*p == 'l') fl = 1; else if (*p == 'w') fw = 1; else if (*p == 'c' || *p == 'm') fc = 1;
            else { pf("wc: invalid option -- '%c'\n", *p); return 1; }
        }
    }
    if (!fl && !fw && !fc) fl = fw = fc = 1;
    int nf = argc - i, rc = 0;
    int nsel = fl + fw + fc;
    if (nf == 0) width = nsel > 1 ? 7 : 0;
    else if (nf == 1 && nsel == 1) width = 0;
    else {
        long sum = 0;
        for (int k = 0; k < nf; k++) { struct stat st; if (stat(argv[i + k], &st) == 0) sum += st.st_size; }
        width = digits((int)sum);
        if (argv[i][0] == '-' && !argv[i][1]) width = 7;
    }
    struct cnt tot = {0, 0, 0};
    for (int k = 0; k < (nf ? nf : 1); k++) {
        const char* name = nf ? argv[i + k] : 0;
        int fd = open_in("wc", name ? name : "-");
        if (fd < 0) { rc = 1; continue; }
        struct cnt t = {0, 0, 0};
        char buf[512];
        int n, inword = 0;
        while ((n = read(fd, buf, sizeof buf)) > 0) {
            for (int j = 0; j < n; j++) {
                char c = buf[j];
                if (c == '\n') t.l++;
                if (isspace((unsigned char)c)) inword = 0;
                else if (!inword) { inword = 1; t.w++; }
            }
            t.c += n;
        }
        if (fd) close(fd);
        show(&t, nf ? name : 0);
        tot.l += t.l; tot.w += t.w; tot.c += t.c;
    }
    if (nf > 1) show(&tot, "total");
    DONE(rc);
}
