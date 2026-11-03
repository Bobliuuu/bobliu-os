#ifndef _SETJMP_H
#define _SETJMP_H 1
typedef int jmp_buf[6];
typedef int sigjmp_buf[6];
#ifdef __cplusplus
extern "C" {
#endif
int setjmp(jmp_buf env) __attribute__((__returns_twice__));
int _setjmp(jmp_buf env) __attribute__((__returns_twice__));
void longjmp(jmp_buf env, int val) __attribute__((__noreturn__));
void _longjmp(jmp_buf env, int val) __attribute__((__noreturn__));
#define sigsetjmp(env, save) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
#ifdef __cplusplus
}
#endif
#endif
