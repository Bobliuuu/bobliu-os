#ifndef _LANGINFO_H
#define _LANGINFO_H 1
typedef int nl_item;
#define CODESET 14
#ifdef __cplusplus
extern "C" {
#endif
char* nl_langinfo(nl_item item);   /* always "UTF-8" for CODESET */
#ifdef __cplusplus
}
#endif
#endif
