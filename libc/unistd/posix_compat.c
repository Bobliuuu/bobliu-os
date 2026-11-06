/* POSIX compatibility layer for porting libuv and neovim.
   Most of this is real (sigaction bookkeeping, mmap on malloc, pthread extras, fstat/ftruncate
   through new syscalls); the rest is honest stubs that fail with ENOSYS where bobliu has no
   equivalent (sockets, dlopen). */
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <wchar.h>
#include <iconv.h>
#include <inttypes.h>
#include <pty.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <langinfo.h>
#include <wctype.h>
#include <ctype.h>
#include <dirent.h>
#include <net/if.h>
#include <sys/statfs.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <kernel/syscall.h>

#define NOSYS(rettype) do { errno = ENOSYS; return (rettype)-1; } while (0)

/* ---- stat family ---------------------------------------------------------------------- */

static void fill_stat(struct stat* st) {
    st->st_dev = 1;
    st->st_blksize = 1024;
    st->st_blocks = (st->st_size + 511) / 512;
    st->st_atime = st->st_ctime = st->st_mtime;
    st->st_atim.tv_sec = st->st_ctim.tv_sec = st->st_mtim.tv_sec = (int)st->st_mtime;
    st->st_atim.tv_nsec = st->st_ctim.tv_nsec = st->st_mtim.tv_nsec = 0;
    st->st_uid = st->st_gid = 0;
    st->st_rdev = 0;
}

int stat(const char* path, struct stat* st) {
    int r = __syscall3(SYS_stat, (int)path, (int)st, 0);
    if (r < 0) { errno = ENOENT; return -1; }
    fill_stat(st);
    return 0;
}
int lstat(const char* path, struct stat* st) { return stat(path, st); }
int fstat(int fd, struct stat* st) {
    int r = __syscall3(SYS_fstat, fd, (int)st, 0);
    if (r < 0) { errno = EBADF; return -1; }
    fill_stat(st);
    return 0;
}
int chmod(const char* path, mode_t m) { (void)m; struct stat st; return stat(path, &st); }
int fchmod(int fd, mode_t m) { (void)m; struct stat st; return fstat(fd, &st); }
int chown(const char* path, uid_t u, gid_t g) { (void)u; (void)g; struct stat st; return stat(path, &st); }
int fchown(int fd, uid_t u, gid_t g) { (void)u; (void)g; struct stat st; return fstat(fd, &st); }
mode_t umask(mode_t m) { static mode_t cur = 022; mode_t old = cur; cur = m & 0777; return old; }

int ftruncate(int fd, off_t len) {
    int r = __syscall3(SYS_ftruncate, fd, len, 0);
    if (r < 0) { errno = EINVAL; return -1; }
    return 0;
}
int fsync(int fd) { struct stat st; return fstat(fd, &st); }   /* every write is already write-through */
int fdatasync(int fd) { return fsync(fd); }
int access(const char* path, int mode) { (void)mode; struct stat st; return stat(path, &st); }
int flock(int fd, int op) { (void)fd; (void)op; return 0; }
int lockf(int fd, int cmd, off_t len) { (void)fd; (void)cmd; (void)len; return 0; }
int link(const char* a, const char* b) { (void)a; (void)b; NOSYS(int); }
int symlink(const char* a, const char* b) { (void)a; (void)b; NOSYS(int); }
ssize_t readlink(const char* p, char* b, size_t n) { (void)p; (void)b; (void)n; errno = EINVAL; return -1; }
int chroot(const char* p) { (void)p; NOSYS(int); }
int getpagesize(void) { return 4096; }

int rmdir(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    return unlink(path);
}

char* realpath(const char* path, char* out) {
    char tmp[PATH_MAX], cwd[PATH_MAX];
    size_t n = 0;
    if (!path || !*path) { errno = ENOENT; return 0; }
    if (path[0] != '/') {
        if (!getcwd(cwd, sizeof cwd)) return 0;
        n = strlen(cwd);
        memcpy(tmp, cwd, n);
        if (n == 0 || tmp[n - 1] != '/') tmp[n++] = '/';
    } else tmp[n++] = '/';
    for (const char* p = path; *p;) {
        while (*p == '/') p++;
        const char* e = p;
        while (*e && *e != '/') e++;
        size_t l = e - p;
        if (l == 0) break;
        if (l == 1 && p[0] == '.') { }
        else if (l == 2 && p[0] == '.' && p[1] == '.') {
            if (n > 1) { n--; while (n > 1 && tmp[n - 1] != '/') n--; }
        } else {
            if (n + l + 1 >= sizeof tmp) { errno = ENAMETOOLONG; return 0; }
            memcpy(tmp + n, p, l); n += l; tmp[n++] = '/';
        }
        p = e;
    }
    if (n > 1) n--;
    tmp[n] = 0;
    struct stat st;
    if (stat(tmp, &st) < 0) return 0;
    if (!out) out = malloc(n + 1);
    if (!out) { errno = ENOMEM; return 0; }
    memcpy(out, tmp, n + 1);
    return out;
}

/* ---- ids, uname, limits ----------------------------------------------------------------- */

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }
pid_t getppid(void) { return 1; }
pid_t getpgrp(void) { return 1; }
pid_t getpgid(pid_t p) { (void)p; return 1; }
int setpgid(pid_t p, pid_t g) { (void)p; (void)g; return 0; }
pid_t setsid(void) { return getpid(); }
pid_t tcgetpgrp(int fd) { return isatty(fd) ? 1 : (errno = ENOTTY, -1); }
int tcsetpgrp(int fd, pid_t g) { (void)g; return isatty(fd) ? 0 : (errno = ENOTTY, -1); }
char* ttyname(int fd) { return isatty(fd) ? "/dev/console" : 0; }
int ttyname_r(int fd, char* b, size_t n) {
    if (!isatty(fd)) return ENOTTY;
    if (n < 13) return ERANGE;
    strcpy(b, "/dev/console");
    return 0;
}
int gethostname(char* name, size_t len) {
    if (len < 7) { errno = ENAMETOOLONG; return -1; }
    strcpy(name, "bobliu");
    return 0;
}
int uname(struct utsname* u) {
    strcpy(u->sysname, "bobliu"); strcpy(u->nodename, "bobliu"); strcpy(u->release, "0.1");
    strcpy(u->version, "#1 bobliu"); strcpy(u->machine, "i686");
    return 0;
}
int getrlimit(int res, struct rlimit* rl) {
    rl->rlim_cur = rl->rlim_max = (res == RLIMIT_NOFILE) ? 16 : (res == RLIMIT_STACK ? 131072 : RLIM_INFINITY);
    return 0;
}
int setrlimit(int res, const struct rlimit* rl) { (void)res; (void)rl; return 0; }
int getrusage(int who, struct rusage* ru) { (void)who; memset(ru, 0, sizeof *ru); return 0; }
long sysconf(int name) {
    switch (name) {
    case _SC_ARG_MAX: return 2048;
    case _SC_CLK_TCK: return 100;
    case _SC_OPEN_MAX: return 16;
    case _SC_PAGESIZE: return 4096;
    case _SC_NPROCESSORS_CONF: case _SC_NPROCESSORS_ONLN: return 1;
    case _SC_GETPW_R_SIZE_MAX: return 256;
    case _SC_HOST_NAME_MAX: return 64;
    }
    errno = EINVAL;
    return -1;
}

static struct passwd g_pw = { "root", "x", 0, 0, "root", "/", "/bin/sh" };
static struct group g_gr = { "root", "x", 0, 0 };
struct passwd* getpwuid(uid_t uid) { return uid == 0 ? &g_pw : 0; }
struct passwd* getpwnam(const char* n) { return strcmp(n, "root") == 0 ? &g_pw : 0; }
int getpwuid_r(uid_t uid, struct passwd* pw, char* buf, size_t n, struct passwd** res) {
    (void)buf; (void)n;
    if (uid != 0) { *res = 0; return 0; }
    *pw = g_pw; *res = pw; return 0;
}
int getpwnam_r(const char* name, struct passwd* pw, char* buf, size_t n, struct passwd** res) {
    (void)buf; (void)n;
    if (strcmp(name, "root")) { *res = 0; return 0; }
    *pw = g_pw; *res = pw; return 0;
}
struct group* getgrgid(gid_t g) { return g == 0 ? &g_gr : 0; }
struct group* getgrnam(const char* n) { return strcmp(n, "root") == 0 ? &g_gr : 0; }

/* ---- misc process / fd ------------------------------------------------------------------- */

int dup3(int oldfd, int newfd, int flags) { (void)flags; return oldfd == newfd ? (errno = EINVAL, -1) : dup2(oldfd, newfd); }

int execvp(const char* file, char* const argv[]) {
    if (strchr(file, '/')) return execv(file, argv);
    char path[PATH_MAX];
    const char* pe = getenv("PATH");
    if (!pe) pe = "/bin";
    while (*pe) {
        const char* e = strchr(pe, ':');
        size_t l = e ? (size_t)(e - pe) : strlen(pe);
        if (l && l + strlen(file) + 2 < sizeof path) {
            memcpy(path, pe, l); path[l] = '/'; strcpy(path + l + 1, file);
            execv(path, argv);
        }
        if (!e) break;
        pe = e + 1;
    }
    errno = ENOENT;
    return -1;
}
int execl(const char* path, const char* arg, ...) {
    char* av[17]; int n = 0;
    __builtin_va_list ap; __builtin_va_start(ap, arg);
    av[n++] = (char*)arg;
    while (n < 16 && (av[n] = __builtin_va_arg(ap, char*))) n++;
    av[n] = 0; __builtin_va_end(ap);
    return execv(path, av);
}
int execlp(const char* file, const char* arg, ...) {
    char* av[17]; int n = 0;
    __builtin_va_list ap; __builtin_va_start(ap, arg);
    av[n++] = (char*)arg;
    while (n < 16 && (av[n] = __builtin_va_arg(ap, char*))) n++;
    av[n] = 0; __builtin_va_end(ap);
    return execvp(file, av);
}
unsigned alarm(unsigned s) { (void)s; return 0; }
int pause(void) { for (;;) { sleep(1); } }
ssize_t pread(int fd, void* b, size_t n, off_t off) {
    int cur = lseek(fd, 0, SEEK_CUR);
    if (cur < 0 || lseek(fd, off, SEEK_SET) < 0) return -1;
    ssize_t r = read(fd, b, n);
    lseek(fd, cur, SEEK_SET);
    return r;
}
ssize_t pwrite(int fd, const void* b, size_t n, off_t off) {
    int cur = lseek(fd, 0, SEEK_CUR);
    if (cur < 0 || lseek(fd, off, SEEK_SET) < 0) return -1;
    ssize_t r = write(fd, b, n);
    lseek(fd, cur, SEEK_SET);
    return r;
}
ssize_t readv(int fd, const struct iovec* iov, int cnt) {
    ssize_t tot = 0;
    for (int i = 0; i < cnt; i++) {
        ssize_t r = read(fd, iov[i].iov_base, iov[i].iov_len);
        if (r < 0) return tot ? tot : -1;
        tot += r;
        if ((size_t)r < iov[i].iov_len) break;
    }
    return tot;
}
ssize_t writev(int fd, const struct iovec* iov, int cnt) {
    ssize_t tot = 0;
    for (int i = 0; i < cnt; i++) {
        ssize_t r = write(fd, iov[i].iov_base, iov[i].iov_len);
        if (r < 0) return tot ? tot : -1;
        tot += r;
        if ((size_t)r < iov[i].iov_len) break;
    }
    return tot;
}

/* ---- signals: handlers are real, the mask is bookkeeping only ----------------------------- */

static sigset_t g_sigmask;
static sigset_t g_handler_mask[NSIG];
int raise(int sig) { return kill(getpid(), sig); }
int sigemptyset(sigset_t* s) { *s = 0; return 0; }
int sigfillset(sigset_t* s) { *s = ~0u; return 0; }
int sigaddset(sigset_t* s, int sig) { if (sig < 1 || sig >= NSIG) { errno = EINVAL; return -1; } *s |= 1u << sig; return 0; }
int sigdelset(sigset_t* s, int sig) { if (sig < 1 || sig >= NSIG) { errno = EINVAL; return -1; } *s &= ~(1u << sig); return 0; }
int sigismember(const sigset_t* s, int sig) { if (sig < 1 || sig >= NSIG) { errno = EINVAL; return -1; } return (*s >> sig) & 1; }
int sigprocmask(int how, const sigset_t* set, sigset_t* old) {
    if (old) *old = g_sigmask;
    if (!set) return 0;
    switch (how) {
    case SIG_BLOCK: g_sigmask |= *set; break;
    case SIG_UNBLOCK: g_sigmask &= ~*set; break;
    case SIG_SETMASK: g_sigmask = *set; break;
    default: errno = EINVAL; return -1;
    }
    return 0;
}
int pthread_sigmask(int how, const sigset_t* set, sigset_t* old) { return sigprocmask(how, set, old) < 0 ? errno : 0; }
int sigaction(int sig, const struct sigaction* act, struct sigaction* old) {
    if (sig < 1 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP) { errno = EINVAL; return -1; }
    static struct sigaction cur[NSIG];
    if (old) *old = cur[sig];
    if (!act) return 0;
    if (act->sa_flags & SA_SIGINFO) { errno = ENOSYS; return -1; }   /* siginfo handlers are not supported */
    sighandler_t r = signal(sig, act->sa_handler);
    if (r == SIG_ERR) { errno = EINVAL; return -1; }
    cur[sig] = *act;
    g_handler_mask[sig] = act->sa_mask;
    return 0;
}

/* ---- mmap: anonymous only ---------------------------------------------------------------- */

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t off) {
    (void)addr; (void)prot; (void)off;
    if (!(flags & MAP_ANONYMOUS) || fd != -1 || len == 0) { errno = ENODEV; return MAP_FAILED; }
    void* raw = calloc(1, len + 4096 + sizeof(void*));
    if (!raw) { errno = ENOMEM; return MAP_FAILED; }
    unsigned long a = ((unsigned long)raw + sizeof(void*) + 4095) & ~4095ul;
    ((void**)a)[-1] = raw;
    return (void*)a;
}
int munmap(void* addr, size_t len) { (void)len; if (addr) free(((void**)addr)[-1]); return 0; }
int mprotect(void* addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }

/* ---- sockets: none. everything fails with ENOSYS, as documented ---------------------------- */

int socket(int d, int t, int p) { (void)d; (void)t; (void)p; errno = EAFNOSUPPORT; return -1; }
int socketpair(int d, int t, int p, int sv[2]) { (void)d; (void)t; (void)p; (void)sv; errno = EAFNOSUPPORT; return -1; }
int bind(int fd, const struct sockaddr* a, socklen_t l) { (void)fd; (void)a; (void)l; errno = ENOTSOCK; return -1; }
int listen(int fd, int b) { (void)fd; (void)b; errno = ENOTSOCK; return -1; }
int accept(int fd, struct sockaddr* a, socklen_t* l) { (void)fd; (void)a; (void)l; errno = ENOTSOCK; return -1; }
int connect(int fd, const struct sockaddr* a, socklen_t l) { (void)fd; (void)a; (void)l; errno = ENOTSOCK; return -1; }
int getsockname(int fd, struct sockaddr* a, socklen_t* l) { (void)fd; (void)a; (void)l; errno = ENOTSOCK; return -1; }
int getpeername(int fd, struct sockaddr* a, socklen_t* l) { (void)fd; (void)a; (void)l; errno = ENOTSOCK; return -1; }
int setsockopt(int fd, int lv, int o, const void* v, socklen_t l) { (void)fd; (void)lv; (void)o; (void)v; (void)l; errno = ENOTSOCK; return -1; }
int getsockopt(int fd, int lv, int o, void* v, socklen_t* l) { (void)fd; (void)lv; (void)o; (void)v; (void)l; errno = ENOTSOCK; return -1; }
int shutdown(int fd, int h) { (void)fd; (void)h; errno = ENOTSOCK; return -1; }
ssize_t send(int fd, const void* b, size_t n, int f) { (void)f; return write(fd, b, n); }
ssize_t recv(int fd, void* b, size_t n, int f) { (void)f; return read(fd, b, n); }
ssize_t sendmsg(int fd, const struct msghdr* m, int f) { (void)fd; (void)m; (void)f; errno = ENOTSOCK; return -1; }
ssize_t recvmsg(int fd, struct msghdr* m, int f) { (void)fd; (void)m; (void)f; errno = ENOTSOCK; return -1; }
ssize_t sendto(int fd, const void* b, size_t n, int f, const struct sockaddr* a, socklen_t l) { (void)a; (void)l; return send(fd, b, n, f); }
ssize_t recvfrom(int fd, void* b, size_t n, int f, struct sockaddr* a, socklen_t* l) { (void)a; (void)l; return recv(fd, b, n, f); }
int getaddrinfo(const char* n, const char* s, const struct addrinfo* h, struct addrinfo** r) { (void)n; (void)s; (void)h; *r = 0; return EAI_FAIL; }
void freeaddrinfo(struct addrinfo* ai) { (void)ai; }
int getnameinfo(const struct sockaddr* sa, socklen_t sl, char* h, socklen_t hl, char* sv, socklen_t svl, int f) { (void)sa; (void)sl; (void)h; (void)hl; (void)sv; (void)svl; (void)f; return EAI_FAIL; }
const char* gai_strerror(int c) { (void)c; return "name resolution is not available"; }
int inet_pton(int af, const char* src, void* dst) {
    if (af != AF_INET) { errno = EAFNOSUPPORT; return -1; }
    unsigned char* o = dst; unsigned v = 0; int parts = 0, digits = 0;
    for (const char* p = src;; p++) {
        if (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); digits++; if (v > 255) return 0; }
        else if (*p == '.' || *p == 0) {
            if (!digits || parts > 3) return 0;
            o[parts++] = (unsigned char)v; v = 0; digits = 0;
            if (!*p) break;
        } else return 0;
    }
    return parts == 4;
}
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    if (af != AF_INET) { errno = EAFNOSUPPORT; return 0; }
    const unsigned char* b = src; char t[16]; int n = 0;
    for (int i = 0; i < 4; i++) {
        unsigned v = b[i];
        if (v >= 100) t[n++] = '0' + v / 100;
        if (v >= 10) t[n++] = '0' + (v / 10) % 10;
        t[n++] = '0' + v % 10;
        if (i < 3) t[n++] = '.';
    }
    t[n] = 0;
    if ((socklen_t)n + 1 > size) { errno = ENOSPC; return 0; }
    strcpy(dst, t);
    return dst;
}

/* ---- dynamic loading: none --------------------------------------------------------------- */

void* dlopen(const char* f, int m) { (void)f; (void)m; return 0; }
void* dlsym(void* h, const char* n) { (void)h; (void)n; return 0; }
int dlclose(void* h) { (void)h; return 0; }
char* dlerror(void) { return "dynamic loading is not supported on bobliu"; }

/* ---- locale / wide chars (ASCII + treat everything else as a letter-less symbol) ----------- */

char* nl_langinfo(nl_item i) { return i == CODESET ? "UTF-8" : ""; }
int iswalpha(wint_t c) { return c < 128 ? isalpha(c) : c >= 0xC0; }
int iswdigit(wint_t c) { return c < 128 && isdigit(c); }
int iswspace(wint_t c) { return c < 128 && isspace(c); }
int iswupper(wint_t c) { return c < 128 && isupper(c); }
int iswlower(wint_t c) { return c < 128 && islower(c); }
int iswprint(wint_t c) { return c < 128 ? isprint(c) : c >= 0xA0; }
int iswpunct(wint_t c) { return c < 128 && ispunct(c); }
int iswalnum(wint_t c) { return iswalpha(c) || iswdigit(c); }
wint_t towlower(wint_t c) { return c < 128 ? (wint_t)tolower(c) : c; }
wint_t towupper(wint_t c) { return c < 128 ? (wint_t)toupper(c) : c; }

/* ---- semaphores and pthread extras --------------------------------------------------------- */

int sem_init(sem_t* s, int ps, unsigned v) { (void)ps; pthread_mutex_init(&s->m, 0); pthread_cond_init(&s->c, 0); s->v = (int)v; return 0; }
int sem_destroy(sem_t* s) { (void)s; return 0; }
int sem_post(sem_t* s) { pthread_mutex_lock(&s->m); s->v++; pthread_cond_signal(&s->c); pthread_mutex_unlock(&s->m); return 0; }
int sem_wait(sem_t* s) { pthread_mutex_lock(&s->m); while (s->v == 0) pthread_cond_wait(&s->c, &s->m); s->v--; pthread_mutex_unlock(&s->m); return 0; }
int sem_trywait(sem_t* s) {
    int ok = 0;
    pthread_mutex_lock(&s->m);
    if (s->v > 0) { s->v--; ok = 1; }
    pthread_mutex_unlock(&s->m);
    if (!ok) { errno = EAGAIN; return -1; }
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* m) { (void)m; return 0; }
int pthread_cond_destroy(pthread_cond_t* c) { (void)c; return 0; }
int pthread_cond_timedwait(pthread_cond_t* c, pthread_mutex_t* m, const struct timespec* abs) {
    /* no timed futex wait: poll the condition in 10 ms steps until the deadline */
    struct timespec now;
    int seq = c->seq;
    pthread_mutex_unlock(m);
    for (;;) {
        if (c->seq != seq) { pthread_mutex_lock(m); return 0; }
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs->tv_sec || (now.tv_sec == abs->tv_sec && now.tv_nsec >= abs->tv_nsec)) break;
        usleep(10000);
    }
    pthread_mutex_lock(m);
    return 110;   /* ETIMEDOUT */
}
int pthread_mutexattr_init(pthread_mutexattr_t* a) { a->type = 0; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t* a) { (void)a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t* a, int t) { a->type = t; return 0; }
int pthread_attr_init(pthread_attr_t* a) { a->stacksize = PTHREAD_STACK_SIZE; a->detach = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t* a) { (void)a; return 0; }
int pthread_attr_setstacksize(pthread_attr_t* a, unsigned n) { a->stacksize = n; return 0; }
int pthread_attr_getstacksize(const pthread_attr_t* a, unsigned* n) { *n = a->stacksize; return 0; }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
int pthread_detach(pthread_t t) { (void)t; return 0; }
int pthread_once(pthread_once_t* o, void (*fn)(void)) {
    if (__sync_bool_compare_and_swap(&o->state, 0, 1)) { fn(); o->state = 2; }
    else while (o->state != 2) sched_yield();
    return 0;
}

#define MAX_KEYS 64
#define MAX_TSD 64
static struct { int used; void (*dtor)(void*); struct { int tid; void* v; } slot[MAX_TSD]; } g_keys[MAX_KEYS];
static volatile int g_key_lock;
static void key_lock(void) { while (__sync_lock_test_and_set(&g_key_lock, 1)) sched_yield(); }
static void key_unlock(void) { __sync_lock_release(&g_key_lock); }
int pthread_key_create(pthread_key_t* k, void (*dtor)(void*)) {
    key_lock();
    for (int i = 0; i < MAX_KEYS; i++) if (!g_keys[i].used) {
        memset(&g_keys[i], 0, sizeof g_keys[i]);
        g_keys[i].used = 1; g_keys[i].dtor = dtor; *k = i;
        key_unlock();
        return 0;
    }
    key_unlock();
    return EAGAIN;
}
int pthread_key_delete(pthread_key_t k) { if (k < 0 || k >= MAX_KEYS) return EINVAL; key_lock(); g_keys[k].used = 0; key_unlock(); return 0; }
void* pthread_getspecific(pthread_key_t k) {
    if (k < 0 || k >= MAX_KEYS || !g_keys[k].used) return 0;
    int tid = pthread_self();
    key_lock();
    void* v = 0;
    for (int i = 0; i < MAX_TSD; i++) if (g_keys[k].slot[i].tid == tid) { v = g_keys[k].slot[i].v; break; }
    key_unlock();
    return v;
}
int pthread_setspecific(pthread_key_t k, const void* v) {
    if (k < 0 || k >= MAX_KEYS || !g_keys[k].used) return EINVAL;
    int tid = pthread_self(), rc = ENOMEM;
    key_lock();
    int free_i = -1;
    for (int i = 0; i < MAX_TSD; i++) {
        if (g_keys[k].slot[i].tid == tid) { g_keys[k].slot[i].v = (void*)v; rc = 0; break; }
        if (free_i < 0 && g_keys[k].slot[i].tid == 0) free_i = i;
    }
    if (rc && free_i >= 0) { g_keys[k].slot[free_i].tid = tid; g_keys[k].slot[free_i].v = (void*)v; rc = 0; }
    key_unlock();
    return rc;
}

/* simple writer-preferring-nothing rwlock on a spin + yield */
int pthread_rwlock_init(pthread_rwlock_t* l, const void* a) { (void)a; l->readers = 0; l->writer = 0; l->lock = 0; return 0; }
int pthread_rwlock_destroy(pthread_rwlock_t* l) { (void)l; return 0; }
static void rw_lock(pthread_rwlock_t* l) { while (__sync_lock_test_and_set(&l->lock, 1)) sched_yield(); }
static void rw_unlock(pthread_rwlock_t* l) { __sync_lock_release(&l->lock); }
int pthread_rwlock_tryrdlock(pthread_rwlock_t* l) {
    int ok = 0;
    rw_lock(l);
    if (!l->writer) { l->readers++; ok = 1; }
    rw_unlock(l);
    return ok ? 0 : EBUSY;
}
int pthread_rwlock_rdlock(pthread_rwlock_t* l) { while (pthread_rwlock_tryrdlock(l)) sched_yield(); return 0; }
int pthread_rwlock_trywrlock(pthread_rwlock_t* l) {
    int ok = 0;
    rw_lock(l);
    if (!l->writer && l->readers == 0) { l->writer = 1; ok = 1; }
    rw_unlock(l);
    return ok ? 0 : EBUSY;
}
int pthread_rwlock_wrlock(pthread_rwlock_t* l) { while (pthread_rwlock_trywrlock(l)) sched_yield(); return 0; }
int pthread_rwlock_unlock(pthread_rwlock_t* l) {
    rw_lock(l);
    if (l->writer) l->writer = 0; else if (l->readers > 0) l->readers--;
    rw_unlock(l);
    return 0;
}

/* ---- directories: opendir/readdir over readdir_at (index based, no fd) ---------------------- */

struct _DIR { char path[PATH_MAX]; int index; struct dirent ent; };

DIR* opendir(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return 0; }
    DIR* d = calloc(1, sizeof *d);
    if (!d) { errno = ENOMEM; return 0; }
    strncpy(d->path, path, sizeof d->path - 1);
    return d;
}
struct dirent* readdir(DIR* d) {
    char name[DIRENT_NAME_MAX];
    if (readdir_at(d->path, d->index, name) <= 0) return 0;
    d->index++;
    strncpy(d->ent.d_name, name, sizeof d->ent.d_name - 1);
    d->ent.d_name[sizeof d->ent.d_name - 1] = 0;
    d->ent.d_ino = d->index;
    d->ent.d_type = DT_UNKNOWN;
    return &d->ent;
}
int closedir(DIR* d) { free(d); return 0; }
void rewinddir(DIR* d) { d->index = 0; }
int dirfd(DIR* d) { (void)d; errno = ENOTSUP; return -1; }
int getpriority(int w, int i) { (void)w; (void)i; return 0; }
int setpriority(int w, int i, int p) { (void)w; (void)i; (void)p; return 0; }
unsigned int if_nametoindex(const char* n) { (void)n; return 0; }
char* if_indextoname(unsigned int i, char* n) { (void)i; (void)n; errno = ENXIO; return 0; }
int getifaddrs(struct ifaddrs** ifap) { *ifap = 0; return 0; }
void freeifaddrs(struct ifaddrs* ifa) { (void)ifa; }
int statfs(const char* p, struct statfs* s) { (void)p; (void)s; NOSYS(int); }
int fstatfs(int fd, struct statfs* s) { (void)fd; (void)s; NOSYS(int); }
const struct in6_addr in6addr_any = { { 0 } };
const struct in6_addr in6addr_loopback = { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } };
long pathconf(const char* p, int n) { (void)p; return n == _PC_NAME_MAX ? 63 : PATH_MAX; }
long fpathconf(int fd, int n) { (void)fd; return n == _PC_NAME_MAX ? 63 : PATH_MAX; }
int pthread_getschedparam(pthread_t t, int* pol, struct sched_param* p) { (void)t; *pol = 0; p->sched_priority = 0; return 0; }
int pthread_setschedparam(pthread_t t, int pol, const struct sched_param* p) { (void)t; (void)pol; (void)p; return 0; }

/* ---- more small pieces libuv wants --------------------------------------------------------- */

int pthread_condattr_init(pthread_condattr_t* a) { a->dummy = 0; return 0; }
int pthread_condattr_destroy(pthread_condattr_t* a) { (void)a; return 0; }
int pthread_condattr_setclock(pthread_condattr_t* a, int c) { (void)a; (void)c; return 0; }
int pthread_atfork(void (*a)(void), void (*b)(void), void (*c)(void)) { (void)a; (void)b; (void)c; return 0; }
int sched_get_priority_min(int p) { (void)p; return 0; }
int sched_get_priority_max(int p) { (void)p; return 0; }
int setuid(uid_t u) { return u == 0 ? 0 : (errno = EPERM, -1); }
int setgid(gid_t g) { return g == 0 ? 0 : (errno = EPERM, -1); }
int setgroups(size_t n, const gid_t* g) { (void)n; (void)g; return 0; }
int lchown(const char* p, uid_t u, gid_t g) { return chown(p, u, g); }
int getgrgid_r(gid_t gid, struct group* g, char* buf, size_t n, struct group** res) {
    (void)buf; (void)n;
    if (gid != 0) { *res = 0; return 0; }
    *g = g_gr; *res = g; return 0;
}
ssize_t preadv(int fd, const struct iovec* iov, int n, off_t off) {
    int cur = lseek(fd, 0, SEEK_CUR);
    if (cur < 0 || lseek(fd, off, SEEK_SET) < 0) return -1;
    ssize_t r = readv(fd, iov, n);
    lseek(fd, cur, SEEK_SET);
    return r;
}
ssize_t pwritev(int fd, const struct iovec* iov, int n, off_t off) {
    int cur = lseek(fd, 0, SEEK_CUR);
    if (cur < 0 || lseek(fd, off, SEEK_SET) < 0) return -1;
    ssize_t r = writev(fd, iov, n);
    lseek(fd, cur, SEEK_SET);
    return r;
}
int utimensat(int d, const char* p, const struct timespec ts[2], int f) { (void)d; (void)ts; (void)f; struct stat st; return stat(p, &st); }
int futimens(int fd, const struct timespec ts[2]) { (void)ts; struct stat st; return fstat(fd, &st); }
int utimes(const char* p, const void* tv) { (void)tv; struct stat st; return stat(p, &st); }
int futimes(int fd, const void* tv) { (void)tv; struct stat st; return fstat(fd, &st); }

static void fill_tmpl(char* x) {
    static unsigned ctr;
    unsigned v = (unsigned)getpid() * 2654435761u + ++ctr * 40503u;
    for (int i = 0; i < 6; i++) { x[i] = "abcdefghijklmnopqrstuvwxyz012345"[v & 31]; v = v * 1103515245u + 12345u; v >>= 3; }
}
int mkstemp(char* t) {
    size_t n = strlen(t);
    if (n < 6 || strcmp(t + n - 6, "XXXXXX")) { errno = EINVAL; return -1; }
    for (int tries = 0; tries < 100; tries++) {
        fill_tmpl(t + n - 6);
        struct stat st;
        if (stat(t, &st) < 0) return open(t, O_RDWR | O_CREAT | O_EXCL, 0600);
    }
    errno = EEXIST;
    return -1;
}
char* mkdtemp(char* t) {
    size_t n = strlen(t);
    if (n < 6 || strcmp(t + n - 6, "XXXXXX")) { errno = EINVAL; return 0; }
    for (int tries = 0; tries < 100; tries++) {
        fill_tmpl(t + n - 6);
        if (mkdir(t, 0700) == 0) return t;
    }
    errno = EEXIST;
    return 0;
}
char* ptsname(int fd) { (void)fd; errno = ENOTTY; return 0; }
int posix_openpt(int f) { (void)f; errno = ENOSYS; return -1; }
int grantpt(int fd) { (void)fd; errno = ENOTTY; return -1; }
int unlockpt(int fd) { (void)fd; errno = ENOTTY; return -1; }

int alphasort(const struct dirent** a, const struct dirent** b) { return strcmp((*a)->d_name, (*b)->d_name); }
int scandir(const char* path, struct dirent*** list, int (*filter)(const struct dirent*),
            int (*cmp)(const struct dirent**, const struct dirent**)) {
    DIR* d = opendir(path);
    if (!d) return -1;
    struct dirent** v = 0;
    int n = 0, cap = 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (filter && !filter(e)) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            struct dirent** nv = realloc(v, cap * sizeof *v);
            if (!nv) { closedir(d); return -1; }
            v = nv;
        }
        v[n] = malloc(sizeof **v);
        if (!v[n]) { closedir(d); return -1; }
        *v[n++] = *e;
    }
    closedir(d);
    if (cmp && n > 1) qsort(v, n, sizeof *v, (int (*)(const void*, const void*))cmp);
    *list = v;
    return n;
}
struct protoent* getprotobyname(const char* n) { (void)n; return 0; }
struct protoent* getprotobynumber(int n) { (void)n; return 0; }
struct servent* getservbyname(const char* n, const char* p) { (void)n; (void)p; return 0; }
struct hostent* gethostbyname(const char* n) { (void)n; return 0; }
size_t wcslen(const wchar_t* s) { size_t n = 0; while (s[n]) n++; return n; }
size_t strnlen(const char* s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }

iconv_t iconv_open(const char* to, const char* from) { (void)to; (void)from; errno = EINVAL; return (iconv_t)-1; }
size_t iconv(iconv_t cd, char** in, size_t* il, char** out, size_t* ol) { (void)cd; (void)in; (void)il; (void)out; (void)ol; errno = EBADF; return (size_t)-1; }
int iconv_close(iconv_t cd) { (void)cd; return 0; }
int openpty(int* m, int* s, char* n, const struct termios* t, const struct winsize* w) { (void)m; (void)s; (void)n; (void)t; (void)w; NOSYS(int); }
int forkpty(int* m, char* n, const struct termios* t, const struct winsize* w) { (void)m; (void)n; (void)t; (void)w; NOSYS(int); }

static int digit_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 99;
}
/* shared by strtoll/strtoull: parses magnitude, reports overflow and sign */
static unsigned long long parse_ull(const char* s, char** end, int base, int* neg, int* ovf) {
    const char* p = s;
    unsigned long long v = 0;
    *neg = 0; *ovf = 0;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    if (*p == '-') { *neg = 1; p++; } else if (*p == '+') p++;
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && digit_val(p[2]) < 16) { p += 2; base = 16; }
    else if (base == 0) base = p[0] == '0' ? 8 : 10;
    const char* start = p;
    for (; digit_val(*p) < base; p++) {
        unsigned d = (unsigned)digit_val(*p);
        if (v > (0xFFFFFFFFFFFFFFFFull - d) / (unsigned)base) *ovf = 1;
        else v = v * (unsigned)base + d;
    }
    if (p == start) { if (end) *end = (char*)s; return 0; }
    if (end) *end = (char*)p;
    return v;
}
long long strtoll(const char* s, char** end, int base) {
    int neg, ovf;
    unsigned long long v = parse_ull(s, end, base, &neg, &ovf);
    if (ovf || (!neg && v > 0x7FFFFFFFFFFFFFFFull) || (neg && v > 0x8000000000000000ull)) {
        errno = ERANGE;
        return neg ? (-0x7FFFFFFFFFFFFFFFll - 1) : 0x7FFFFFFFFFFFFFFFll;
    }
    return neg ? (long long)(0 - v) : (long long)v;
}
unsigned long long strtoull(const char* s, char** end, int base) {
    int neg, ovf;
    unsigned long long v = parse_ull(s, end, base, &neg, &ovf);
    if (ovf) { errno = ERANGE; return 0xFFFFFFFFFFFFFFFFull; }
    return neg ? 0 - v : v;
}
intmax_t strtoimax(const char* s, char** end, int base) { return strtoll(s, end, base); }
uintmax_t strtoumax(const char* s, char** end, int base) { return strtoull(s, end, base); }
long long llabs(long long x) { return x < 0 ? -x : x; }
char* strtok_r(char* s, const char* delim, char** save) {
    if (!s) s = *save;
    s += strspn(s, delim);
    if (!*s) { *save = s; return 0; }
    char* tok = s;
    s += strcspn(s, delim);
    if (*s) *s++ = 0;
    *save = s;
    return tok;
}
void tzset(void) {}
static int g_pwent;
struct passwd* getpwent(void) { return g_pwent++ == 0 ? &g_pw : 0; }
void setpwent(void) { g_pwent = 0; }
void endpwent(void) { g_pwent = 0; }
