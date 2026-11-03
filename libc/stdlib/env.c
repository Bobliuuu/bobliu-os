// process environment: the kernel puts envp after argv, crt0 stores it in environ
#include <stdlib.h>
#include <string.h>

char** environ;
static int g_owned;               // environ array is ours (malloc'd) and may be resized

static int env_count(void) { int n = 0; if (environ) while (environ[n]) n++; return n; }

static int own(int extra) {
    int n = env_count();
    char** e = malloc((n + extra + 1) * sizeof(char*));
    if (!e) return -1;
    for (int i = 0; i < n; i++) e[i] = environ[i];
    e[n] = 0;
    if (g_owned) free(environ);
    environ = e;
    g_owned = 1;
    return 0;
}

static int find(const char* name, size_t len) {
    if (!environ) return -1;
    for (int i = 0; environ[i]; i++)
        if (!strncmp(environ[i], name, len) && environ[i][len] == '=') return i;
    return -1;
}

char* getenv(const char* name) {
    int i = find(name, strlen(name));
    return i < 0 ? 0 : strchr(environ[i], '=') + 1;
}

int setenv(const char* name, const char* value, int overwrite) {
    size_t nl = strlen(name);
    if (!nl || strchr(name, '=')) return -1;
    int i = find(name, nl);
    if (i >= 0 && !overwrite) return 0;
    char* s = malloc(nl + strlen(value) + 2);
    if (!s) return -1;
    memcpy(s, name, nl); s[nl] = '=';
    strcpy(s + nl + 1, value);
    if (i >= 0) { if (!g_owned && own(0) < 0) { free(s); return -1; } environ[i] = s; return 0; }
    if (own(1) < 0) { free(s); return -1; }
    int n = env_count();
    environ[n] = s; environ[n + 1] = 0;
    return 0;
}

int putenv(char* str) {
    char* eq = strchr(str, '=');
    if (!eq) return -1;
    char name[128];
    size_t nl = eq - str;
    if (nl >= sizeof(name)) return -1;
    memcpy(name, str, nl); name[nl] = 0;
    return setenv(name, eq + 1, 1);
}

int unsetenv(const char* name) {
    int i = find(name, strlen(name));
    if (i < 0) return 0;
    if (!g_owned && own(0) < 0) return -1;
    for (; environ[i]; i++) environ[i] = environ[i + 1];
    return 0;
}

int clearenv(void) {
    if (own(0) < 0) return -1;
    environ[0] = 0;
    return 0;
}
