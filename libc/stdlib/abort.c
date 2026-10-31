#include <stdio.h>
#include <stdlib.h>
#if !defined(__is_libk)
#include <unistd.h>
#endif

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	// TODO: Add proper kernel panic.
	printf("kernel: panic: abort()\n");
        asm volatile("hlt");
#else
	printf("abort()\n");
	_exit(134);
#endif
	while (1) { }
	__builtin_unreachable();
}