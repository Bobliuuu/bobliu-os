#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/tty.h>

// If you have timer ticks exported:
extern uint32_t timer_ticks(void);

// Optional: if you implement this later
// extern void pmm_stats(void);

static void shell_prompt(void) {
    terminal_clear_eol();
    printf("bobliu> ");
}

// very small tokenizer: splits by spaces into argv
static int shell_tokenize(char* s, char* argv[], int max_argv) {
    int argc = 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (argc >= max_argv) break;
        argv[argc++] = s;
        while (*s && *s != ' ') s++;
        if (*s) *s++ = '\0';
    }
    return argc;
}

static void cmd_help(void) {
    printf("commands:\n");
    printf("  help            - show this\n");
    printf("  clear           - clear screen\n");
    printf("  echo <text...>  - print text\n");
    printf("  ticks           - show timer ticks\n");
    printf("  panic           - trigger a crash (test)\n");
}

static void cmd_clear(void) {
    terminal_initialize();
}

static void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i + 1 < argc) printf(" ");
    }
    printf("\n");
}

static void cmd_ticks(void) {
    printf("ticks=%u\n", timer_ticks());
}

static void cmd_panic(void) {
    //volatile int x = 1 / 0;
    __asm__ volatile ("ud2");
}

void shell_init(void) {
    printf("\n");
    printf("Kernel shell ready. Type 'help'.\n");
    shell_prompt();
}

// called by keyboard.c on Enter
void shell_on_line(const char* line_in) {
    printf("\n");
    // Make a mutable copy for tokenizing
    char line[128];
    size_t n = strlen(line_in);
    if (n >= sizeof(line)) n = sizeof(line) - 1;
    memcpy(line, line_in, n);
    line[n] = '\0';

    // empty line => just re-prompt
    if (line[0] == '\0') {
        shell_prompt();
        return;
    }

    char* argv[16];
    int argc = shell_tokenize(line, argv, 16);
    if (argc == 0) {
        shell_prompt();
        return;
    }

    if (strcmp(argv[0], "help") == 0) {
        cmd_help();
    } else if (strcmp(argv[0], "clear") == 0) {
        cmd_clear();
    } else if (strcmp(argv[0], "echo") == 0) {
        cmd_echo(argc, argv);
    } else if (strcmp(argv[0], "ticks") == 0) {
        cmd_ticks();
    } else if (strcmp(argv[0], "panic") == 0) {
        cmd_panic();
    } else {
        printf("unknown command: %s\n", argv[0]);
    }

    shell_prompt();
}
