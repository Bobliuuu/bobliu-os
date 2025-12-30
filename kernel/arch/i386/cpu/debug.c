#include <stdint.h>
#include <stdio.h>

void exec_debug(uint32_t entry, uint32_t user_stack_top, uint32_t kesp, uint32_t resume) {
    printf("exec: entry=%x user_stack_top=%x\n", entry, user_stack_top);
    printf("exec: saved_kesp=%x resume=%x\n", kesp, resume);
}