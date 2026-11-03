#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(void) {
    struct termios t, old;
    struct winsize ws;
    printf("isatty 0=%d 1=%d\n", isatty(0), isatty(1));
    if (tcgetattr(0, &old) < 0) { printf("tcgetattr FAIL\n"); return 1; }
    printf("lflag canon=%d echo=%d isig=%d\n", !!(old.c_lflag & ICANON), !!(old.c_lflag & ECHO), !!(old.c_lflag & ISIG));
    ioctl(1, TIOCGWINSZ, &ws);
    printf("size %dx%d\n", ws.ws_col, ws.ws_row);
    printf("\033[31mred\033[0m \033[1;32mbold green\033[0m \033[44;37mwhite on blue\033[0m \033[7minverse\033[0m\n");
    printf("AAAA\033[2D\033[BXX\033[A\033[3CY\n\n");
    printf("12345\033[3D\033[2P|\033[@+\n");
    t = old;
    cfmakeraw(&t);
    t.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &t);
    printf("raw: press 4 keys\r\n");
    for (int i = 0; i < 4; i++) {
        unsigned char c;
        if (read(0, &c, 1) != 1) break;
        printf("key %d\r\n", c);
    }
    tcsetattr(0, TCSANOW, &old);
    printf("cooked again\n");
    char line[64];
    t = old; t.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &t);
    printf("secret (no echo): ");
    fflush(stdout);
    fgets(line, sizeof line, stdin);
    tcsetattr(0, TCSANOW, &old);
    printf("\ngot %d chars\n", (int)strlen(line) - 1);
    return 0;
}
