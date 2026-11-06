#ifndef _STDINT_H
#define _STDINT_H

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef long long int intmax_t;
typedef unsigned long long int uintmax_t;
typedef long int intptr_t;
typedef unsigned long int uintptr_t;

#define UINT8_MAX  0xff
#define UINT16_MAX 0xffff
#define UINT32_MAX 0xffffffffu
#define UINT64_MAX 0xffffffffffffffffull

#define INT8_MIN   (-128)
#define INT16_MIN  (-32768)
#define INT32_MIN  (-2147483647 - 1)
#define INT64_MIN  (-9223372036854775807ll - 1)

#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  9223372036854775807ll

#define INTMAX_MAX INT64_MAX
#define INTMAX_MIN (-INT64_MAX - 1)
#define UINTMAX_MAX UINT64_MAX
#define INTPTR_MAX 0x7fffffff
#define UINTPTR_MAX 0xffffffffu
#ifndef SIZE_MAX
#define SIZE_MAX 0xFFFFFFFFu
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX 0x7FFFFFFF
#endif
#ifndef PTRDIFF_MAX
#define PTRDIFF_MAX 0x7FFFFFFF
#endif
#endif
