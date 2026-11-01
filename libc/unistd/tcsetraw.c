#include <unistd.h>
#include <kernel/syscall.h>

int tcsetraw(int on) {
    return __syscall3(SYS_tcsetraw, on, 0, 0);
}
