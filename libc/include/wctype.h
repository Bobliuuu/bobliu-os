#ifndef _WCTYPE_H
#define _WCTYPE_H 1
#include <stddef.h>
typedef int wint_t;
#ifdef __cplusplus
extern "C" {
#endif
int iswalpha(wint_t c);
int iswdigit(wint_t c);
int iswspace(wint_t c);
int iswupper(wint_t c);
int iswlower(wint_t c);
int iswprint(wint_t c);
int iswpunct(wint_t c);
int iswalnum(wint_t c);
wint_t towlower(wint_t c);
wint_t towupper(wint_t c);
#ifdef __cplusplus
}
#endif
#endif
