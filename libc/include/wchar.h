#ifndef _WCHAR_H
#define _WCHAR_H 1
#include <stddef.h>
#include <wctype.h>
#define WEOF ((wint_t)-1)
typedef struct { int __count; unsigned __value; } mbstate_t;
#ifdef __cplusplus
extern "C" {
#endif
size_t wcslen(const wchar_t* s);
#ifdef __cplusplus
}
#endif
#endif
