#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#define EOF (-1)
#define BUFSIZ 4096
#define FILENAME_MAX 256
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* __restrict, ...);
int vprintf(const char* fmt, __builtin_va_list ap);
int putchar(int);
int puts(const char*);

/* formatted output to buffers */
int vsnprintf(char* buf, size_t size, const char* fmt, __builtin_va_list ap);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, __builtin_va_list ap);
int sscanf(const char* s, const char* fmt, ...);   /* %d %i %u %x %o %s %c %n %f %lf %g %e, %*, widths on %s; no sets */

/* buffered files (libc/stdio/file.c) */
typedef struct _FILE FILE;
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* path, const char* mode);
FILE* freopen(const char* path, const char* mode, FILE* f);
int fclose(FILE* f);
size_t fread(void* buf, size_t size, size_t n, FILE* f);
size_t fwrite(const void* buf, size_t size, size_t n, FILE* f);
int fseek(FILE* f, long off, int whence);
long ftell(FILE* f);
void rewind(FILE* f);
int fflush(FILE* f);               /* fflush(NULL) flushes every open file */
int feof(FILE* f);
int ferror(FILE* f);
void clearerr(FILE* f);
int fgetc(FILE* f);
int getc(FILE* f);
int getchar(void);
int ungetc(int c, FILE* f);
char* fgets(char* s, int n, FILE* f);
FILE* fdopen(int fd, const char* mode);
int getline(char** lineptr, size_t* n, FILE* f);   /* returns length or -1 */
int fputc(int c, FILE* f);
int putc(int c, FILE* f);
int fputs(const char* s, FILE* f);
int fprintf(FILE* f, const char* fmt, ...);
int vfprintf(FILE* f, const char* fmt, __builtin_va_list ap);
int fscanf(FILE* f, const char* fmt, ...);   /* same subset as sscanf */
int fileno(FILE* f);
void setbuf(FILE* f, char* buf);
int setvbuf(FILE* f, char* buf, int mode, size_t size);
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define L_tmpnam 32
#define TMP_MAX 10000
char* tmpnam(char* buf);
FILE* tmpfile(void);
int remove(const char* path);
int rename(const char* from, const char* to);   /* copies then unlinks (no rename syscall) */
void perror(const char* s);

#ifdef __cplusplus
}
#endif

#endif
