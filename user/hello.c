#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    puts("hello from userland!");
    printf("pid=%d strlen=%d hex=%x str=%s\n", getpid(), (int)strlen("bobliu"), 0xBEEF, "libc works");
    return 0;
}
