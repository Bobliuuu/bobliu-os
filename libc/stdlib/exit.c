#include <stdlib.h>
#include <unistd.h>

// no atexit handlers or stdio buffers to flush yet
void exit(int status) {
    _exit(status);
}
