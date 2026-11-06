/* exercises the POSIX compatibility layer added for the libuv / neovim port */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <unistd.h>

static int total, bad;
#define CHECK(name, cond) do { total++; if (!(cond)) { bad++; printf("FAIL %s (errno %d)\n", name, errno); } } while (0)

static volatile int got_sig;
static void on_usr1(int s) { got_sig = s; }
static pthread_once_t once = PTHREAD_ONCE_INIT;
static int once_n;
static void once_fn(void) { once_n++; }

int main(void) {
    struct stat st;
    /* fstat / stat / ftruncate */
    int fd = open("/tmp/px.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    CHECK("open", fd >= 0);
    CHECK("write", write(fd, "hello world", 11) == 11);
    CHECK("fstat", fstat(fd, &st) == 0 && st.st_size == 11 && S_ISREG(st.st_mode));
    CHECK("stat", stat("/tmp/px.txt", &st) == 0 && st.st_size == 11 && S_ISREG(st.st_mode));
    CHECK("stat dir", stat("/tmp", &st) == 0 && S_ISDIR(st.st_mode));
    CHECK("stat missing", stat("/tmp/nope", &st) < 0 && errno == ENOENT);
    CHECK("ftruncate 0", ftruncate(fd, 0) == 0 && fstat(fd, &st) == 0 && st.st_size == 0);
    CHECK("fstat console", fstat(1, &st) == 0 && S_ISCHR(st.st_mode));
    CHECK("access", access("/tmp/px.txt", F_OK) == 0 && access("/tmp/nope", F_OK) < 0);
    CHECK("fsync", fsync(fd) == 0);
    close(fd);
    CHECK("rename", rename("/tmp/px.txt", "/tmp/px2.txt") == 0 && access("/tmp/px2.txt", F_OK) == 0 && access("/tmp/px.txt", F_OK) < 0);
    CHECK("mkdir 2 args", mkdir("/tmp/pxd", 0755) == 0 && rmdir("/tmp/pxd") == 0);
    unlink("/tmp/px2.txt");

    /* realpath */
    char rp[256];
    CHECK("realpath", realpath("/tmp/../tmp/./", rp) && strcmp(rp, "/tmp") == 0);

    /* signals */
    struct sigaction sa, old;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_usr1;
    sigemptyset(&sa.sa_mask);
    CHECK("sigaction", sigaction(SIGUSR1, &sa, &old) == 0);
    CHECK("raise", raise(SIGUSR1) == 0);
    for (int i = 0; i < 100 && !got_sig; i++) usleep(1000);
    CHECK("handler ran", got_sig == SIGUSR1);
    sigset_t s, o;
    sigemptyset(&s); sigaddset(&s, SIGINT);
    CHECK("sigprocmask", sigprocmask(SIG_BLOCK, &s, &o) == 0 && sigprocmask(SIG_SETMASK, &o, &s) == 0 && sigismember(&s, SIGINT) == 1);

    /* mmap */
    char* m = mmap(0, 10000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK("mmap", m != MAP_FAILED && ((unsigned long)m & 4095) == 0 && m[0] == 0 && m[9999] == 0);
    if (m != MAP_FAILED) { m[5] = 7; CHECK("munmap", munmap(m, 10000) == 0); }
    CHECK("mmap file fails", mmap(0, 10, PROT_READ, MAP_PRIVATE, 0, 0) == MAP_FAILED);

    /* ids, uname, sysconf */
    struct utsname u;
    CHECK("uname", uname(&u) == 0 && strcmp(u.sysname, "bobliu") == 0);
    struct passwd* pw = getpwuid(getuid());
    CHECK("getpwuid", pw && strcmp(pw->pw_name, "root") == 0);
    CHECK("sysconf", sysconf(_SC_PAGESIZE) == 4096 && sysconf(_SC_NPROCESSORS_ONLN) == 1);

    /* sockets are absent, and say so */
    CHECK("socket fails", socket(AF_INET, SOCK_STREAM, 0) < 0 && errno == EAFNOSUPPORT);
    unsigned char ip[4];
    CHECK("inet_pton", inet_pton(AF_INET, "127.0.0.1", ip) == 1 && ip[0] == 127 && ip[3] == 1);
    char ipbuf[16];
    CHECK("inet_ntop", inet_ntop(AF_INET, ip, ipbuf, sizeof ipbuf) && strcmp(ipbuf, "127.0.0.1") == 0);

    /* pthread extras */
    pthread_once(&once, once_fn); pthread_once(&once, once_fn);
    CHECK("pthread_once", once_n == 1);
    pthread_key_t k;
    CHECK("key create", pthread_key_create(&k, 0) == 0);
    CHECK("key set/get", pthread_setspecific(k, (void*)0x1234) == 0 && pthread_getspecific(k) == (void*)0x1234);
    pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
    CHECK("rwlock", pthread_rwlock_rdlock(&rw) == 0 && pthread_rwlock_tryrdlock(&rw) == 0 && pthread_rwlock_trywrlock(&rw) != 0 &&
          pthread_rwlock_unlock(&rw) == 0 && pthread_rwlock_unlock(&rw) == 0 && pthread_rwlock_wrlock(&rw) == 0 && pthread_rwlock_unlock(&rw) == 0);
    sem_t sem;
    sem_init(&sem, 0, 1);
    CHECK("sem", sem_trywait(&sem) == 0 && sem_trywait(&sem) < 0 && sem_post(&sem) == 0 && sem_wait(&sem) == 0);

    printf("posixtest: %d/%d ok\n", total - bad, total);
    return bad ? 1 : 0;
}
