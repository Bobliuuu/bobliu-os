#ifndef _ICONV_H
#define _ICONV_H 1
#include <stddef.h>
typedef void* iconv_t;
#ifdef __cplusplus
extern "C" {
#endif
iconv_t iconv_open(const char* to, const char* from);   /* no conversion tables: always fails with EINVAL */
size_t iconv(iconv_t cd, char** in, size_t* inleft, char** out, size_t* outleft);
int iconv_close(iconv_t cd);
#ifdef __cplusplus
}
#endif
#endif
