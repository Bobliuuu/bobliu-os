#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("write(NULL) -> %d\n", write(1, 0, 5));          // kernel must reject, not die
    printf("execv(missing) -> %d\n", execv("/nope", 0));
    printf("about to segfault\n");
    *(volatile int*)0 = 1;
    printf("not reached\n");
    return 0;
}
