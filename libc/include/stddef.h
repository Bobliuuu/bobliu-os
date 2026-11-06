#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL ((void*)0)

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#define offsetof(t, m) __builtin_offsetof(t, m)
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

#endif
