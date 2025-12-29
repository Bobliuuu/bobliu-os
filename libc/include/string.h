#ifndef _STRING_H
#define _STRING_H 1

#include <sys/cdefs.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
size_t strlen(const char*);
int strcmp(const char* a, const char* b);
char* strcpy(char* restrict dest, const char* restrict src);
char* strstr(const char* haystack, const char* needle);
int strncmp(const char* a, const char* b, size_t n);
char* strncpy(char* dst, const char* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif