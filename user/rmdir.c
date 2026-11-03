#include "common.h"
int main(int argc, char** argv) {
    if (argc < 2) { pf("rmdir: missing operand\n"); return 1; }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (!exists(argv[i])) { err("rmdir", argv[i], "No such file or directory"); rc = 1; }
        else if (!is_dir(argv[i])) { err("rmdir", argv[i], "Not a directory"); rc = 1; }
        else if (unlink(argv[i]) < 0) { err("rmdir", argv[i], "failed to remove (not empty?)"); rc = 1; }
    }
    return rc;
}
