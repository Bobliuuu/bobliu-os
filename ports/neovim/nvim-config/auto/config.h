#pragma once
/* neovim config.h for bobliu (i686, plain Lua 5.1, no gettext). Hand-written: cmake's
   check_c_source_compiles results are for the host, so each HAVE_* is decided against bobliu's libc. */
#define SIZEOF_INT 4
#define SIZEOF_INTMAX_T 8
#define SIZEOF_LONG 4
#define SIZEOF_SIZE_T 4
#define ARCH_32
#define PROJECT_NAME "nvim"
#define HAVE_FD_CLOEXEC
#define HAVE_LANGINFO_H
#define HAVE_NL_LANGINFO_CODESET
#define HAVE_PWD_FUNCS
#define HAVE_READLINK
#define HAVE_STRNLEN
#define HAVE_STRCASECMP
#define HAVE_STRINGS_H
#define HAVE_STRNCASECMP
#define HAVE_SYS_UTSNAME_H
#define HAVE_SYS_WAIT_H
#define HAVE_TERMIOS_H
#define UNIX
#define HAVE_SYS_UIO_H
#define HAVE_READV
#define ENDIAN_INCLUDE_FILE <endian.h>
#define HAVE_BUILTIN_ADD_OVERFLOW
#define HAVE_WIMPLICIT_FALLTHROUGH_FLAG
