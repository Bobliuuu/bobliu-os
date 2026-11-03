// env: print the environment; env NAME=value ... [cmd args]: run cmd with a modified one
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int i = 1;
    for (; i < argc && strchr(argv[i], '=') && argv[i][0] != '='; i++) putenv(argv[i]);
    if (i < argc) {
        char path[160];
        if (strchr(argv[i], '/')) strcpy(path, argv[i]);
        else { strcpy(path, "/bin/"); strcat(path, argv[i]); }
        execv(path, argv + i);
        printf("env: %s: command not found\n", argv[i]);
        return 127;
    }
    for (char** e = environ; e && *e; e++) printf("%s\n", *e);
    return 0;
}
