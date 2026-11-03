// 1 MiB of initialised data: checks that the exec loader has no size limit
#include <stdio.h>
static char pad[1 << 20] = { 1, 2, 3 };
int main(void) {
    unsigned sum = 0;
    for (int i = 0; i < (int)sizeof(pad); i += 4096) sum += pad[i];
    printf("bigtest ok (%u, pad[2]=%d)\n", sum, pad[2]);
    return 0;
}
