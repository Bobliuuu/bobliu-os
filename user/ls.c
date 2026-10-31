#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "/";
    char name[DIRENT_NAME_MAX];

    for (int i = 0;; i++) {
        int r = readdir_at(dir, i, name);
        if (r < 0) { printf("ls: %s: cannot read directory\n", dir); return 1; }
        if (r == 0) break;

        char full[256];
        strcpy(full, dir);
        if (full[strlen(full) - 1] != '/') strcat(full, "/");
        strcat(full, name);

        struct stat st;
        if (stat(full, &st) == 0 && st.st_isdir) printf("%s/\n", name);
        else if (stat(full, &st) == 0)          printf("%s  (%d bytes)\n", name, (int)st.st_size);
        else                                    printf("%s\n", name);
    }
    return 0;
}
