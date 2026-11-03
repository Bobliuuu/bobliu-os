#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__))
void abort(void);
__attribute__((__noreturn__))
void exit(int status);

void* malloc(size_t size);
void* calloc(size_t n, size_t size);
void* realloc(void* p, size_t size);
void  free(void* p);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7fffffff
int atexit(void (*fn)(void));          /* up to 32; run by exit() */
int abs(int x);
long labs(long x);
int atoi(const char* s);
long atol(const char* s);
double atof(const char* s);
double strtod(const char* s, char** end);   /* decimal, inf, nan, hex ints */
float strtof(const char* s, char** end);
long strtol(const char* s, char** end, int base);
unsigned long strtoul(const char* s, char** end, int base);
int rand(void);
void srand(unsigned seed);
extern char** environ;                 /* set up by crt0 from the kernel's initial stack */
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int putenv(char* string);
int unsetenv(const char* name);
int clearenv(void);
int system(const char* cmd);           /* no shell to call: -1 */
void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif