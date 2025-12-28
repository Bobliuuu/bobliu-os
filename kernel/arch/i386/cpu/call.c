#include <stdint.h>

typedef void (*ctor_t)(void);

/* New style */
extern ctor_t __init_array_start[];
extern ctor_t __init_array_end[];

/* Old style (.ctors is an array of function pointers) */
extern uintptr_t __ctors_start[];
extern uintptr_t __ctors_end[];

static void run_init_array(void) {
    for (ctor_t* f = __init_array_start; f < __init_array_end; f++) {
        if (*f) (*f)();
    }
}

void call_global_constructors(void) {
    // Walk backwards: common layout is:
    // [ -1 ][ ctorN ][ ... ][ ctor0 ]
    for (uintptr_t* p = __ctors_end; p != __ctors_start; ) {
        --p;
        uintptr_t v = *p;

        // Skip sentinel and nulls
        if (v == 0 || v == (uintptr_t)-1) continue;

        ((ctor_t)v)();
    }
}
