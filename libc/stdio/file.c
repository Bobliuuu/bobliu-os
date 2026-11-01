// buffered stdio over the fd syscalls
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define F_READ   1
#define F_WRITE  2
#define F_APPEND 4
#define F_EOF    8
#define F_ERR    16
#define F_UNBUF  32     // every write goes straight out (stdout, stderr)
#define F_STATIC 64

struct _FILE {
    int fd;
    int flags;
    unsigned char* buf;       // BUFSIZ bytes, allocated on first use
    int rpos, rend;           // unread bytes: buf[rpos..rend)
    int wlen;                 // pending write bytes: buf[0..wlen)
    int unget;                // pushed back char or -1
    struct _FILE* next;       // list of open files for fflush(NULL)
};

static FILE g_in  = { 0, F_READ | F_STATIC, 0, 0, 0, 0, -1, 0 };
static FILE g_out = { 1, F_WRITE | F_UNBUF | F_STATIC, 0, 0, 0, 0, -1, 0 };
static FILE g_err = { 2, F_WRITE | F_UNBUF | F_STATIC, 0, 0, 0, 0, -1, 0 };
FILE* stdin = &g_in;
FILE* stdout = &g_out;
FILE* stderr = &g_err;

static FILE* g_open;

static int ensure_buf(FILE* f) {
    if (!f->buf) f->buf = malloc(BUFSIZ);
    return f->buf != 0;
}

static int flush_write(FILE* f) {
    int off = 0;
    while (off < f->wlen) {
        int r = write(f->fd, f->buf + off, f->wlen - off);
        if (r <= 0) { f->flags |= F_ERR; f->wlen = 0; return EOF; }
        off += r;
    }
    f->wlen = 0;
    return 0;
}

// drop buffered read data, moving the fd back to where the program thinks it is
static void drop_read(FILE* f) {
    if (f->rend > f->rpos) lseek(f->fd, -(f->rend - f->rpos), SEEK_CUR);
    f->rpos = f->rend = 0;
    f->unget = -1;
}

FILE* fopen(const char* path, const char* mode) {
    int flags = 0, fl = 0;
    int plus = strchr(mode, '+') != 0;
    switch (mode[0]) {
    case 'r': flags = plus ? O_RDWR : O_RDONLY; fl = F_READ | (plus ? F_WRITE : 0); break;
    case 'w': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC; fl = F_WRITE | (plus ? F_READ : 0); break;
    case 'a': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND; fl = F_WRITE | F_APPEND | (plus ? F_READ : 0); break;
    default: errno = EINVAL; return 0;
    }
    int fd = open(path, flags);
    if (fd < 0) { errno = ENOENT; return 0; }
    FILE* f = calloc(1, sizeof(FILE));
    if (!f) { close(fd); errno = ENOMEM; return 0; }
    f->fd = fd; f->flags = fl; f->unget = -1;
    f->next = g_open; g_open = f;
    return f;
}

int fflush(FILE* f) {
    if (!f) {
        int r = 0;
        for (FILE* p = g_open; p; p = p->next) if (p->wlen && flush_write(p) < 0) r = EOF;
        return r;
    }
    return f->wlen ? flush_write(f) : 0;
}

int fclose(FILE* f) {
    int r = fflush(f);
    if (f->flags & F_STATIC) return r;
    if (close(f->fd) < 0) r = EOF;
    for (FILE** pp = &g_open; *pp; pp = &(*pp)->next)
        if (*pp == f) { *pp = f->next; break; }
    free(f->buf);
    free(f);
    return r;
}

static int fill(FILE* f) {
    if (f->wlen && flush_write(f) < 0) return -1;
    if (!ensure_buf(f)) return -1;
    int r = read(f->fd, f->buf, BUFSIZ);
    if (r < 0) { f->flags |= F_ERR; return -1; }
    if (r == 0) { f->flags |= F_EOF; return 0; }
    f->rpos = 0; f->rend = r;
    return r;
}

size_t fread(void* dst, size_t size, size_t n, FILE* f) {
    size_t want = size * n, got = 0;
    char* d = dst;
    if (!want || !(f->flags & F_READ)) return 0;
    if (f->unget >= 0) { *d++ = (char)f->unget; f->unget = -1; got++; }
    while (got < want) {
        if (f->rpos == f->rend) {
            if (want - got >= BUFSIZ && !f->wlen) {            // big read: skip the buffer
                int r = read(f->fd, (char*)dst + got, (int)(want - got));
                if (r < 0) { f->flags |= F_ERR; break; }
                if (r == 0) { f->flags |= F_EOF; break; }
                got += r;
                continue;
            }
            if (fill(f) <= 0) break;
        }
        size_t take = f->rend - f->rpos;
        if (take > want - got) take = want - got;
        memcpy((char*)dst + got, f->buf + f->rpos, take);
        f->rpos += take; got += take;
    }
    return got / size;
}

size_t fwrite(const void* src, size_t size, size_t n, FILE* f) {
    size_t len = size * n;
    if (!len || !(f->flags & F_WRITE)) return 0;
    if (f->rend > f->rpos || f->unget >= 0) drop_read(f);
    if (f->flags & F_UNBUF) {
        const char* p = src;
        size_t off = 0;
        while (off < len) {
            int r = write(f->fd, p + off, len - off);
            if (r <= 0) { f->flags |= F_ERR; return off / size; }
            off += r;
        }
        return n;
    }
    if (!ensure_buf(f)) return 0;
    const char* p = src;
    size_t off = 0;
    while (off < len) {
        if (f->wlen == BUFSIZ && flush_write(f) < 0) return off / size;
        size_t take = BUFSIZ - f->wlen;
        if (take > len - off) take = len - off;
        memcpy(f->buf + f->wlen, p + off, take);
        f->wlen += take; off += take;
    }
    return n;
}

long ftell(FILE* f) {
    int pos = lseek(f->fd, 0, SEEK_CUR);
    if (pos < 0) return -1;
    return pos - (f->rend - f->rpos) - (f->unget >= 0) + f->wlen;
}

int fseek(FILE* f, long off, int whence) {
    if (f->wlen && flush_write(f) < 0) return -1;
    if (whence == SEEK_CUR) { off += ftell(f); whence = SEEK_SET; }
    f->rpos = f->rend = 0;
    f->unget = -1;
    f->flags &= ~F_EOF;
    return lseek(f->fd, (int)off, whence) < 0 ? -1 : 0;
}

void rewind(FILE* f) { fseek(f, 0, SEEK_SET); f->flags &= ~F_ERR; }

int feof(FILE* f) { return (f->flags & F_EOF) != 0; }
int ferror(FILE* f) { return (f->flags & F_ERR) != 0; }
void clearerr(FILE* f) { f->flags &= ~(F_EOF | F_ERR); }
int fileno(FILE* f) { return f->fd; }

int fgetc(FILE* f) {
    unsigned char c;
    if (f->unget >= 0) { int u = f->unget; f->unget = -1; return u; }
    if (f->rpos == f->rend && fill(f) <= 0) return EOF;
    c = f->buf[f->rpos++];
    return c;
}
int getc(FILE* f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }

int ungetc(int c, FILE* f) {
    if (c == EOF || f->unget >= 0) return EOF;
    f->unget = (unsigned char)c;
    f->flags &= ~F_EOF;
    return c;
}

char* fgets(char* s, int n, FILE* f) {
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) break;
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    s[i] = 0;
    return s;
}

int fputc(int c, FILE* f) {
    unsigned char ch = (unsigned char)c;
    return fwrite(&ch, 1, 1, f) == 1 ? ch : EOF;
}
int putc(int c, FILE* f) { return fputc(c, f); }

int fputs(const char* s, FILE* f) {
    size_t n = strlen(s);
    return fwrite(s, 1, n, f) == n ? 0 : EOF;
}

int vfprintf(FILE* f, const char* fmt, va_list ap) {
    char small[256];
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(small, sizeof(small), fmt, ap);
    if (n < (int)sizeof(small)) {
        if (n > 0) fwrite(small, 1, n, f);
    } else {
        char* big = malloc(n + 1);
        if (big) { vsnprintf(big, n + 1, fmt, ap2); fwrite(big, 1, n, f); free(big); }
    }
    va_end(ap2);
    return n;
}

int fprintf(FILE* f, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int fscanf(FILE* f, const char* fmt, ...) { (void)f; (void)fmt; return EOF; }

void setbuf(FILE* f, char* buf) { (void)f; (void)buf; }
int setvbuf(FILE* f, char* buf, int mode, size_t size) {
    (void)buf; (void)size;
    if (mode == _IONBF) f->flags |= F_UNBUF;
    return 0;
}

int remove(const char* path) { return unlink(path); }

int rename(const char* from, const char* to) {
    FILE* a = fopen(from, "rb");
    if (!a) return -1;
    FILE* b = fopen(to, "wb");
    if (!b) { fclose(a); return -1; }
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), a)) > 0) fwrite(buf, 1, n, b);
    fclose(a);
    int bad = fclose(b);
    if (bad) return -1;
    return unlink(from);
}

void perror(const char* s) { fprintf(stderr, "%s: error %d\n", s ? s : "", errno); }
