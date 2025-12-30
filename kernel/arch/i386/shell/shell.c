#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <kernel/vfs.h>
#include <kernel/tty.h>
#include <kernel/user_exec.h>
#include <arch/i386/keyboard.h>
#include <arch/i386/user_bouncing.h>
#include <arch/i386/isr.h>
#include <arch/i386/portio.h>
#include <arch/i386/paging.h>

extern volatile uint32_t g_exec_kcr3;

// If you have timer ticks exported:
extern uint32_t timer_ticks(void);
// Shell commands implemented in /shell
int cmd_mem(int argc, char** argv);
int cmd_alloc(int argc, char** argv);
void initrd_ls(void);
int  initrd_cat(const char* path);
// Optional: to debug pmm pages
// extern void pmm_stats(void);
extern void keyboard_enable_shell(bool enable);
extern void keyboard_irq(regs_t* r);

extern void irq_install_handler(int irq, void (*fn)(regs_t*));
static void irq1_scream(regs_t* r) { (void)r; putchar('K'); }

static inline uint32_t read_cr3(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

#define SHELL_MAX_PATH 256
static char g_cwd[SHELL_MAX_PATH] = "/";

typedef int (*cmd_fn)(int argc, char** argv);

typedef struct {
    const char* name;
    cmd_fn fn;
} cmd_t;

static int cmd_help(int argc, char** argv);
static int cmd_clear(int argc, char** argv);
static int cmd_echo(int argc, char** argv);
static int cmd_ticks(int argc, char** argv);
static int cmd_uptime(int argc, char** argv);
static int cmd_panic(int argc, char** argv);
// new ones
static int cmd_pwd(int argc, char** argv);
static int cmd_cd(int argc, char** argv);
static int cmd_ls(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_hexdump(int argc, char** argv);
static int cmd_exec(int argc, char** argv);

static const cmd_t cmds[] = {
    { "help",    cmd_help },
    { "clear",   cmd_clear },
    { "echo",    cmd_echo },
    { "ticks",   cmd_ticks },
    { "uptime",  cmd_uptime },
    { "mem",     cmd_mem },
    { "alloc",   cmd_alloc },
    { "pwd",     cmd_pwd },
    { "cd",      cmd_cd },
    { "ls",      cmd_ls },
    { "cat",     cmd_cat },
    { "hexdump", cmd_hexdump },
    { "exec", cmd_exec },
    { "panic",   cmd_panic },
};
static const int cmd_count = (int)(sizeof(cmds)/sizeof(cmds[0]));

static void shell_set_cwd(const char* s) {
    if (!s || !s[0]) { strcpy(g_cwd, "/"); return; }
    size_t n = strlen(s);
    if (n >= SHELL_MAX_PATH) n = SHELL_MAX_PATH - 1;
    memcpy(g_cwd, s, n);
    g_cwd[n] = '\0';
}

// Turn a path into canonical absolute path.
// Rules:
// - output always starts with '/'
// - collapses '//' -> '/'
// - handles '.' and '..'
// - if input is relative, it's applied to cwd
static void shell_canon_path(char out[SHELL_MAX_PATH], const char* cwd, const char* in) {
    char tmp[SHELL_MAX_PATH];

    // 1) build absolute candidate into tmp
    if (!in || !in[0]) in = ".";
    if (in[0] == '/') {
        strncpy(tmp, in, sizeof(tmp));
        tmp[sizeof(tmp)-1] = '\0';
    } else {
        if (!cwd || !cwd[0]) cwd = "/";
        if (strcmp(cwd, "/") == 0) {
            snprintf(tmp, sizeof(tmp), "/%s", in);
        } else {
            snprintf(tmp, sizeof(tmp), "%s/%s", cwd, in);
        }
    }

    // 2) normalize into out using a component stack
    int comp_starts[64];
    int depth = 0;

    // start with root
    out[0] = '/';
    out[1] = '\0';
    size_t out_len = 1;

    // walk tmp, extracting components
    const char* p = tmp;
    while (*p) {
        while (*p == '/') p++; 
        if (!*p) break;

        // read one component [p, q)
        const char* q = p;
        while (*q && *q != '/') q++;
        size_t n = (size_t)(q - p);
        // component string is p..q-1
        if (n == 1 && p[0] == '.') {
            // ignore "."
        } else if (n == 2 && p[0] == '.' && p[1] == '.') {
            // pop ".." if possible
            if (depth > 0) {
                depth--;
                out_len = (size_t)comp_starts[depth];
                out[out_len] = '\0';
                // trim trailing slash
            } else {
                // already at root: ignore
            }
        } else {
            // push normal component
            if (depth < (int)(sizeof(comp_starts)/sizeof(comp_starts[0]))) {
                // ensure space: maybe need '/' + comp + '\0'
                if (out_len + 1 + n + 1 < SHELL_MAX_PATH) {
                    // add slash if not root-only
                    if (out_len > 1) {
                        out[out_len++] = '/';
                        out[out_len] = '\0';
                    }
                    comp_starts[depth++] = (int)out_len; // start of this component
                    memcpy(out + out_len, p, n);
                    out_len += n;
                    out[out_len] = '\0';
                }
            }
        }
        p = q;
    }
    // special case: if nothing but root
    if (out_len == 0) {
        strcpy(out, "/");
    }
}

// DEPRECATED FOR shell_canon_path
// join cwd + arg into out 
static void shell_resolve_path(char* out, size_t out_sz, const char* in) {
    if (!in || !in[0]) { strncpy(out, g_cwd, out_sz); out[out_sz-1]='\0'; return; }

    if (in[0] == '/') {
        strncpy(out, in, out_sz);
        out[out_sz-1] = '\0';
        return;
    }

    // cwd == "/" ? "/in" : "cwd/in"
    if (strcmp(g_cwd, "/") == 0) {
        snprintf(out, out_sz, "/%s", in);
    } else {
        snprintf(out, out_sz, "%s/%s", g_cwd, in);
    }
}

static void shell_prompt(void) {
    //terminal_clear_eol();
    printf("bobliu:%s> ", g_cwd);
}

void shell_prompt_public(void) {
    shell_prompt();
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

// Already implements global ctor tests
static int cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("commands:\n");
    printf("  help            - show this\n");
    printf("  clear           - clear screen\n");
    printf("  echo <text...>  - print text\n");
    printf("  ticks           - show timer ticks\n");
    printf("  mem             - show physical memory stats\n");
    printf("  alloc <bytes>   - kmalloc test\n");
    printf("  pwd             - print cwd\n");
    printf("  cd [path]       - change directory\n");
    printf("  ls [path]       - list directory\n");
    printf("  cat <path>      - print file\n");
    printf("  hexdump <path>  - dump bytes\n");
    printf("  panic           - trigger crash\n");
    return 0;
}

static int cmd_clear(int argc, char** argv) { 
    (void)argc;
    (void)argv; 
    terminal_initialize(); 
    return 0; 
}
static int cmd_ticks(int argc, char** argv) { 
    (void)argc;
    (void)argv; 
    printf("ticks=%u\n", timer_ticks()); 
    return 0; 
}
static int cmd_panic(int argc, char** argv) { 
    (void)argc;
    (void)argv; 
    __asm__ volatile("ud2"); 
    return 0; 
}

static int cmd_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    uint32_t t = timer_ticks();
    // if your PIT is 100Hz, seconds = ticks/100
    uint32_t sec = t / 100;
    printf("uptime=%u s\n", sec);
    return 0;
}

static int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i + 1 < argc) printf(" ");
    }
    printf("\n");
    return 0;
}

static int cmd_pwd(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("%s\n", g_cwd);
    return 0;
}

static int cmd_cd(int argc, char** argv) {
    const char* arg = (argc >= 2) ? argv[1] : "/";

    char path[SHELL_MAX_PATH];
    shell_canon_path(path, g_cwd, arg);

    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0 || !st.is_dir) {
        printf("cd: no such directory: %s\n", path);
        return -1;
    }

    shell_set_cwd(path);
    return 0;
}

static int cmd_ls(int argc, char** argv) {
    const char* arg = (argc >= 2) ? argv[1] : ".";

    char path[SHELL_MAX_PATH];
    shell_canon_path(path, g_cwd, arg);

    if (vfs_ls(path) < 0) {
        printf("ls: failed: %s\n", path);
        return -1;
    }
    return 0;
}

static int cmd_cat(int argc, char** argv) {
    if (argc < 2) { printf("usage: cat <path>\n"); return -1; }

    char path[256];
    shell_canon_path(path, g_cwd, argv[1]);

    int fd = vfs_open(path);
    if (fd < 0) {
        printf("cat: open failed: %s\n", path);
        return -1;
    }

    char buf[256];
    for (;;) {
        int r = vfs_read(fd, buf, sizeof(buf));
        if (r <= 0) break;
        for (int i = 0; i < r; i++) putchar(buf[i]);
    }
    vfs_close(fd);
    putchar('\n');
    return 0;
}

// optional: show raw bytes (very useful for debugging)
static int cmd_hexdump(int argc, char** argv) {
    if (argc < 2) { printf("usage: hexdump <path>\n"); return -1; }

    char path[256];
    shell_canon_path(path, g_cwd, argv[1]);

    int fd = vfs_open(path);
    if (fd < 0) { printf("hexdump: open failed: %s\n", path); return -1; }

    uint8_t buf[16];
    uint32_t off = 0;

    for (;;) {
        int r = vfs_read(fd, (char*)buf, sizeof(buf));
        if (r <= 0) break;

        printf("%08x: ", off);
        for (int i = 0; i < 16; i++) {
            if (i < r) printf("%02x ", buf[i]);
            else       printf("   ");
        }
        printf(" |");
        for (int i = 0; i < r; i++) {
            char c = (buf[i] >= 32 && buf[i] <= 126) ? (char)buf[i] : '.';
            putchar(c);
        }
        printf("|\n");

        off += (uint32_t)r;
        if (r < (int)sizeof(buf)) break;
    }

    vfs_close(fd);
    return 0;
}

static int cmd_exec(int argc, char** argv) {
    if (argc < 2) { printf("usage: exec <path>\n"); return -1; }

    char path[256];
    shell_canon_path(path, g_cwd, argv[1]);

    if (user_exec(path) < 0) {
        printf("exec: failed: %s\n", path);
        return -1;
    }
    keyboard_enable_shell(true);
    printf("\n");
    printf("FINISHED\n");
    shell_prompt_public();
    return 0; // never reached on success
}

void shell_init(void) {
    //printf("\n");
    printf("Kernel shell ready. Type 'help'.\n");
    shell_prompt();
}

// called by keyboard.c on Enter
void shell_on_line(const char* line_in) {
    printf("\n");

    char line[128];
    size_t n = strlen(line_in);
    if (n >= sizeof(line)) n = sizeof(line) - 1;
    memcpy(line, line_in, n);
    line[n] = '\0';

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

    bool ok = false;
    for (int i = 0; i < cmd_count; i++) {
        if (strcmp(argv[0], cmds[i].name) == 0) {
            cmds[i].fn(argc, argv);
            ok = true;
            break;
        }
    }
    if (!ok) {
        printf("unknown command: %s\n", argv[0]);
    }

    shell_prompt();
}

__attribute__((noreturn))
void exec_return_to_shell(void) {
    printf("cr3=%x (saved kcr3=%x)\n", read_cr3(), g_exec_kcr3);
    ps2_enable_irq1_only();
    pic_unmask_irq1();
    keyboard_enable_shell(true);
    printf("\n");
    shell_prompt_public();

    irq_install_handler(1, irq1_scream);
    __asm__ volatile("sti");          // <<< ADD THIS
    for (;;) {
        //if (inb(0x64) & 1) {      // output buffer full
            //uint8_t sc = inb(0x60);
            //printf("{%x}", sc);
        //}
        __asm__ volatile("hlt");
    }
}