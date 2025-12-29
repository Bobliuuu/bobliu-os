#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stddef.h>   // size_t

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* __restrict, ...);
int putchar(int);
int puts(const char*);

/* formatted output to buffers */
int vsnprintf(char* buf, size_t size, const char* fmt, __builtin_va_list ap);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif