#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

static int fail(const char* w) { printf("threadtest FAIL: %s\n", w); return 1; }

static int counter;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void* add_fn(void* arg) {
    int n = (int)arg;
    for (int i = 0; i < n; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    return (void*)(n + 1);
}

static void* square(void* arg) { return (void*)((int)arg * (int)arg); }

static int pfd[2];
static void* pipe_writer(void* arg) { write(pfd[1], "from thread", 11); return arg; }

static int ready, data;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static void* producer(void* arg) {
    (void)arg;
    sleep(1);
    pthread_mutex_lock(&lock);
    data = 42; ready = 1;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&lock);
    return 0;
}

static void* exiter(void* arg) { exit((int)arg); }
static void* spinner(void* arg) { (void)arg; for (;;); }
static void* chdirer(void* arg) { (void)arg; return (void*)chdir("/tmp"); }
static void* grow(void* arg) { (void)arg; char* p = malloc(100000); memset(p, 7, 100000); return p; }

int main(void) {
    char buf[32];
    int st;

    // many threads bumping one counter under a mutex
    pthread_t t[4];
    for (int i = 0; i < 4; i++) if (pthread_create(&t[i], 0, add_fn, (void*)5000)) return fail("create");
    for (int i = 0; i < 4; i++) {
        void* r;
        if (pthread_join(t[i], &r) || (int)r != 5001) return fail("join value");
    }
    if (counter != 20000) { printf("counter=%d\n", counter); return fail("mutex"); }
    printf("4 threads x 5000 increments under a mutex: ok\n");

    // return values and create/join churn (slots and stacks get reused)
    for (int i = 0; i < 100; i++) {
        pthread_t x; void* r;
        if (pthread_create(&x, 0, square, (void*)i) || pthread_join(x, &r) || (int)r != i * i) return fail("churn");
    }
    printf("100 create/join cycles: ok\n");

    // threads share the fd table
    pipe(pfd);
    pthread_t w;
    pthread_create(&w, 0, pipe_writer, 0);
    memset(buf, 0, sizeof(buf));
    if (read(pfd[0], buf, 11) != 11 || strcmp(buf, "from thread") != 0) return fail("shared fds");
    pthread_join(w, 0);
    close(pfd[0]); close(pfd[1]);
    printf("shared fd table: ok\n");

    // shared cwd, shared heap (thread's malloc visible here)
    pthread_t c, g; void* r;
    pthread_create(&c, 0, chdirer, 0);
    pthread_join(c, 0);
    char cwd[32];
    getcwd(cwd, sizeof(cwd));
    if (strcmp(cwd, "/tmp") != 0) return fail("shared cwd");
    chdir("/");
    pthread_create(&g, 0, grow, 0);
    pthread_join(g, &r);
    if (((char*)r)[99999] != 7) return fail("shared heap");
    free(r);
    printf("shared cwd and heap: ok\n");

    // condition variable
    pthread_t p;
    pthread_create(&p, 0, producer, 0);
    pthread_mutex_lock(&lock);
    while (!ready) pthread_cond_wait(&cv, &lock);
    int got = data;
    pthread_mutex_unlock(&lock);
    pthread_join(p, 0);
    if (got != 42) return fail("cond");
    printf("condition variable: ok\n");

    // exit() in a worker ends the whole process, including the spinning main thread
    int pid = fork();
    if (pid == 0) {
        pthread_t e;
        pthread_create(&e, 0, exiter, (void*)9);
        for (;;);
    }
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 9) return fail("exit from worker");
    printf("exit() from a worker thread: ok\n");

    // SIGKILL to a process with spinning threads takes all of them down and frees the slots
    pid = fork();
    if (pid == 0) {
        pthread_t s1, s2;
        pthread_create(&s1, 0, spinner, 0);
        pthread_create(&s2, 0, spinner, 0);
        for (;;);
    }
    sleep(1);
    kill(pid, SIGKILL);
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 128 + SIGKILL) return fail("kill group");
    pthread_t many[20];
    for (int i = 0; i < 20; i++) if (pthread_create(&many[i], 0, square, (void*)i)) return fail("slots leaked");
    for (int i = 0; i < 20; i++) pthread_join(many[i], 0);
    printf("kill takes the whole group: ok\n");

    // fork from a thread copies only that thread; exec is refused while threads exist
    pthread_t sp;
    pthread_create(&sp, 0, spinner, 0);
    char* args[] = { "/bin/hello", 0 };
    if (execv("/bin/hello", args) != -1) return fail("exec with live threads");
    pid = fork();
    if (pid == 0) {
        execv("/bin/hello", args);                 // the child has only this one thread, so exec works
        exit(1);
    }
    waitpid(pid, &st, 0);
    if (WEXITSTATUS(st) != 0) return fail("fork copies one thread");
    printf("fork + exec rules: ok\n");

    printf("threadtest done\n");
    exit(0);                                         // takes the spinner down too
}
