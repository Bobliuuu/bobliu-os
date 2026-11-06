#ifndef _INTTYPES_H
#define _INTTYPES_H 1
#include <stdint.h>
/* i686: int is 32 bits, long is 32 bits, long long is 64 bits */
#define PRId8  "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"
#define PRIdMAX "lld"
#define PRIdPTR "d"
#define PRIi8  "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "lli"
#define PRIiMAX "lli"
#define PRIu8  "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"
#define PRIuMAX "llu"
#define PRIuPTR "u"
#define PRIo8  "o"
#define PRIo16 "o"
#define PRIo32 "o"
#define PRIo64 "llo"
#define PRIoMAX "llo"
#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "llx"
#define PRIxMAX "llx"
#define PRIxPTR "x"
#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "llX"
#define PRIXMAX "llX"
#define PRIXPTR "X"
#define SCNd32 "d"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNd64 "lld"
#define SCNu64 "llu"
#define SCNx64 "llx"
typedef struct { intmax_t quot, rem; } imaxdiv_t;
#ifdef __cplusplus
extern "C" {
#endif
intmax_t strtoimax(const char* s, char** end, int base);
uintmax_t strtoumax(const char* s, char** end, int base);
#ifdef __cplusplus
}
#endif
#endif
