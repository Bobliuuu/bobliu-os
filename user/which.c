// which cmd...: search /bin
#include "common.h"
int main(int argc, char** argv) {
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        char p[PATH_MAX_];
        if (strchr(argv[i], '/')) { strncpy(p, argv[i], sizeof p - 1); p[sizeof p - 1] = 0; }
        else path_join(p, sizeof p, "/bin", argv[i]);
        if (exists(p) && !is_dir(p)) puts_nl(p); else rc = 1;
    }
    DONE(rc);
}
