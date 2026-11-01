#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void __run_atexit(void);

// run atexit handlers, flush every stdio file, leave
void exit(int status) {
    __run_atexit();
    fflush(NULL);
    _exit(status);
}
