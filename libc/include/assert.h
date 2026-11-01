#ifndef _ASSERT_H
#define _ASSERT_H 1
void __assert_fail(const char* expr, const char* file, int line) __attribute__((__noreturn__));
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__))
#endif
#endif
