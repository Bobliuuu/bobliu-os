#include <stddef.h>

typedef void (*ctor_t)(void);

// Provided by linker.ld
extern ctor_t __init_array_start[];
extern ctor_t __init_array_end[];

void call_global_constructors(void) {
    for (ctor_t* f = __init_array_start; f != __init_array_end; f++) {
        (*f)();
    }
}