#ifndef _DLFCN_H
#define _DLFCN_H 1
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
#ifdef __cplusplus
extern "C" {
#endif
void* dlopen(const char* file, int mode);   /* no dynamic linking: always fails */
void* dlsym(void* h, const char* name);
int dlclose(void* h);
char* dlerror(void);
#ifdef __cplusplus
}
#endif
#endif
