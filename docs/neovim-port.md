# Neovim port plan

Target: neovim v0.10.4 on bobliu, cross-built with `i686-bobliu-gcc`. Sources and scratch builds live
outside the repo (`~/bobliu-os-work/nvim-port`); only patches, stubs and build scripts land here.

## What is already in place

Lua 5.1, libm, float printf/strtod, `setjmp`, `scanf`, environment, termios + ANSI terminal,
`poll`, `O_NONBLOCK`, `pipe`, `fork`/`execve`, `waitpid`, threads + futex, signals (`kill`,
`signal`), `clock_gettime`, `nanosleep`, exec loader without a size limit.

## Dependencies neovim needs

| Dependency | Plan |
|---|---|
| libuv 1.48 | Build with the generic `posix-poll.c` backend (no epoll/kqueue), `pipe` instead of `socketpair` for the async wakeup, sockets/DNS stubbed to `ENOSYS` |
| luv | Lua bindings for libuv, built against bobliu Lua 5.1 |
| Lua + `bit` | Use Lua 5.1 with LuaBitOp as the `bit` module (no LuaJIT, no `ffi`) |
| msgpack-c | Plain C, needs only libc |
| unibilium | Terminfo reader. Ship a `bobliu` terminfo entry matching the ANSI subset in `tty.c` |
| libvterm | Pure C, for `:terminal` (can be disabled for the first boot) |
| tree-sitter runtime | Pure C. Parsers optional |
| lpeg | Pure C, needs only the Lua API |
| utf8proc | Not needed: neovim v0.10.4 vendors its own Unicode tables in `src/unicode` |

## Kernel and libc gaps (milestone 1)

Headers missing from libc that libuv or neovim include: `sys/socket.h`, `netinet/in.h`,
`arpa/inet.h`, `netdb.h`, `sys/un.h`, `sys/uio.h`, `sys/resource.h`, `sys/param.h`, `sys/utsname.h`,
`sys/file.h`, `pwd.h`, `grp.h`, `dlfcn.h`, `sched.h`, `semaphore.h`, `limits.h`, `langinfo.h`,
`libintl.h`, `iconv.h`, `wctype.h`, `float.h`, `stdatomic.h`.

Syscalls and functions to add or stub: `sigaction`, `sigprocmask`, `sigemptyset`/`sigaddset`,
`socketpair` (backed by two pipes or `ENOSYS`), `mmap`/`munmap` (anonymous only), `ftruncate`,
`fsync`, `rename`, `getuid`/`getpwuid`, `uname`, `getrlimit`, `sysconf`, `realpath`, `access`,
`select`, `setsid`, `isatty` on every fd, `getenv`-style locale stubs, `dlopen` stubs returning an error.

## Milestones

| # | Milestone | Done when |
|---|---|---|
| 1 | libc headers and kernel syscalls above | **Done.** `posixtest` passes 35/35 under QEMU; existing tests unchanged |
| 2 | libuv builds and runs | **Done.** `uvtest` (timers, pipes, thread async wakeup, threadpool fs) and `uvtest2` (spawn with a piped child, exit callback, tty init/winsize/raw mode) pass under QEMU |
| 3 | Pure-C deps (msgpack, unibilium, libvterm, tree-sitter + 6 parsers, lpeg, luv) | **Done.** `depstest` passes 12/12 under QEMU |
| 4 | Host codegen | **Done.** `gen-nvim-sources.sh` produces 363 generated headers on the host, 0 failures |
| 5 | Cross-build `nvim` | **Done.** 186 neovim sources compile and link into a 4.8 MB `nvim` against bobliu's libc |
| 6 | `nvim --version`, then `nvim --headless -c 'q'` | **Done.** Both run under QEMU; `--headless -c q` exits 0 with no messages |
| 7 | Interactive editing | **Done.** The TUI starts, inserts text, leaves insert mode and runs `:wq`; `cat` shows the saved file. Syntax highlighting is partial (see below) |

## Risks

- Binary size: the initrd is a tar loaded as a module, so a ~10 MB `nvim` plus runtime files is fine, but the exec loader and the 16-process limit need checking under load.
- Memory: the kernel heap is 4 MiB and user `brk` heap is capped at 256 MiB per process. Neovim allocates heavily at startup.
- No `mmap` file mappings and no sockets: anything in neovim that opens a server socket (`--listen`, RPC over TCP) will not work. Embedded RPC over stdio/pipes should.
- LuaJIT-only features: some runtime Lua uses `ffi`. Those paths need patches or stubs.

## Milestone 1 notes

- Kernel: new `fstat` and `ftruncate` syscalls (`ftruncate` supports length 0 or the current size), and `stat` now returns `mode`, `mtime`, `ino` and `nlink`. `mtime` is the current time, not a per-file time.
- libc: about 25 new headers (`sys/socket.h`, `netinet/in.h`, `pwd.h`, `dlfcn.h`, `sys/mman.h`, ...), a full Linux `errno` set, `sigaction` and the sigset functions (handlers are real, the mask is only recorded), anonymous `mmap` on `malloc`, pthread extras (keys, once, rwlock, attrs, timed wait), semaphores, `getpwuid`, `uname`, `realpath`, `access`, `execvp`.
- Sockets, DNS and `dlopen` are stubs that fail. `socketpair` fails with `EAFNOSUPPORT`, so libuv's `uv_spawn` needs a patch to use two pipes (milestone 2).
- Known gaps: no file `mmap`, no per-file times, `sigaction` rejects `SA_SIGINFO`, `rename` is copy then unlink and only works for files.

## Milestone 2 notes

Build with `ports/neovim/build-libuv.sh` (clones libuv v1.48.0, applies `patches/libuv-0001-spawn-use-pipes.patch`, builds `libuv.a` and the two tests into `initrd/bin`). libuv is compiled with `-D__GNU__`, which selects its generic POSIX path: `poll()` backend, no epoll/kqueue, no proctitle.

Kernel and libc changes this needed, beyond milestone 1:

- `SIGCHLD` is sent to the parent when a child exits. Its default action, like `SIGWINCH`, is to ignore.
- `waitpid` now distinguishes `EINTR` from `ECHILD` (the kernel returns -4 for an interrupted wait).
- `fcntl(F_GETFL)` reports the access mode (`O_RDONLY`/`O_WRONLY`/`O_RDWR`) and `O_APPEND`, not just `O_NONBLOCK`. libuv's `uv_pipe_open` uses this to decide whether a handle is readable or writable.
- `/dev/null`, `/dev/zero`, `/dev/urandom` and `/dev/random` exist. `urandom` is a xorshift generator seeded from the tick counter, so it is not cryptographic.
- `fstat` reports `S_IFIFO` for pipes and `S_IFCHR` for the console and these devices, so `uv_guess_handle` classifies fds correctly.
- libc `open` sets `errno`; `fdopen`, `opendir`/`readdir`/`scandir`, `mkstemp`, `offsetof`, `wchar_t` were missing and are added.
- libuv patch: `uv_spawn` cannot use `socketpair`, so a piped stdio container uses a one-way `pipe()`. A container that asks for both directions fails with `UV_ENOTSUP`.

Known gaps: `FD_CLOEXEC` is recorded nowhere (`F_SETFD` is a no-op), so `uv_spawn` returns only after the child exits when it leaves the exec-status pipe open. Sockets, `getaddrinfo` and `dlopen` still fail.

## Milestone 3 notes

`ports/neovim/build-deps.sh` downloads the pinned sources and builds `libmsgpack-c.a`, `libunibilium.a`, `libvterm.a`, `liblpeg.a`, `libluv.a`, `libtree-sitter.a`, `libts-parsers.a` (c, lua, vim, vimdoc, query, markdown, markdown_inline) and `liblua.a` into `$WORK/prefix`, then links `depstest` against all of them. `depstest` checks a msgpack round trip, a unibilium dump/load, libvterm screen text, an lpeg match, a Lua script running libuv timers through luv, and one parse per tree-sitter language.

- libuv gained `ports/neovim/libuv-bobliu.c`: `uv_uptime` is real, `uv_exepath` and `uv_cpu_info` return `UV_ENOSYS`, memory and load figures are zero, and there are no network interfaces.
- neovim vendors `bit`, cjson, klib, mpack, termkey, xdiff and Unicode tables itself, so those are built with neovim in milestone 5.
- libc gained `strnlen`, `wchar.h`, `getprotobyname` (always NULL) and the missing `AI_*`, `SIZE_MAX` constants.
- The vimdoc parser reports a missing `<` on any non-empty text. A native build of the same parser does the same, so `depstest` checks vimdoc loosely.

## Milestone 4 notes

`ports/neovim/gen-nvim-sources.sh` clones neovim v0.10.4, builds msgpack-c and libvterm from neovim's own bundled recipes, configures the host build against Homebrew's LuaJIT, libuv, luv, lpeg, unibilium and tree-sitter, and runs `ninja generated-sources`. The result is 363 `*.generated.h` files (function prototypes, the API dispatch tables, the embedded Lua runtime, Unicode tables, option and event tables) under `neovim/build/{include,src/nvim/auto}`.

- `COMPILE_LUA=OFF`: the embedded Lua runtime is stored as source text, since the generator's LuaJIT bytecode is not loadable by bobliu's Lua 5.1.
- The generated prototype headers come from running the host C preprocessor over neovim's own sources, so they describe neovim's functions only. `config.h` is the part that depends on the target, and milestone 5 writes that by hand for bobliu.
- Neovim's bundled LuaJIT fails to build on this Mac (an arm64 link error), which is why the host build uses Homebrew's.

## Milestone 5 notes

`ports/neovim/build-nvim.sh` compiles the 186 C files in `ports/neovim/nvim-sources.txt` (the exact set neovim's own cmake build uses for `nvim_bin`, taken from `ninja -t commands`) with `i686-bobliu-gcc` and links them with the dependency archives. `ports/neovim/nvim-config/auto/` holds the hand-written `config.h`, `pathdef.h` and `versiondef.h` (32-bit, no gettext, no xattr, no forkpty).

What the compile found missing in libc, all now added: `intmax_t` and the `PRI*` macros, `iconv.h` (every conversion fails), `pty.h` (fails with ENOSYS), `strtoll`/`strtoull`/`strtoimax`, `llabs`, `strtok_r`, `tzset`, `getpwent`, `fpclassify` and `FP_*`, more `termios` control characters, `O_NOFOLLOW`, `F_DUPFD_CLOEXEC`, `S_IREAD`/`S_IWRITE`, `luv/luv.h`.

Not done yet: the tree-sitter parsers are linked statically, but neovim loads parsers with `dlopen`, so none are registered. That is a patch for later.

## Milestone 6 and 7 notes

Verified under QEMU: `nvim --version` prints `NVIM v0.10.4 ... Lua 5.1`; `nvim --headless -c q` exits 0 silently; and

    nvim --headless -c 'edit /tmp/hi.txt' -c 'normal ihello from nvim on bobliu' -c write -c q

writes `"~/hi.txt" [New] 1L, 26B written`, after which `cat /tmp/hi.txt` prints `hello from nvim on bobliu`. That exercises the Vim engine, the embedded Lua runtime, the event loop, buffers, normal mode and file writing.

What it took beyond milestone 5:

- `ports/neovim/stage-runtime.sh` puts a 3.3 MB runtime (Lua modules, filetype detection, core and a few language syntax scripts, colour schemes) into `initrd/share/nvim/runtime`; `pathdef.h` points `VIMRUNTIME` there.
- `sh` now defaults `HOME` to `/tmp` (`/` is read-only, and nvim writes its ShaDa and swap state under `$HOME`) and sets `NVIM_LOG_FILE=/tmp/nvim.log`.
- The VGA terminal's escape parser was rewritten: it swallows private-marker sequences such as `ESC[>4;0m`, intermediate bytes, `ESC ( B` and OSC strings instead of printing the tail as text.

Remaining warnings in a headless run are harmless: no server socket (`address family not supported`, so no RPC over a socket), and `tempdir root has invalid permissions` because bobliu has no permission bits (nvim falls back to `/tmp/XXXXXX`).

What fixed the TUI (it had been exiting at once):

- `fcntl(fd, F_DUPFD_CLOEXEC, n)` was unknown to the kernel and returned -1; neovim's `--embed` child hit an assertion in `event/stream.c`. The kernel now handles it.
- `FD_CLOEXEC` was a no-op. libuv's `uv_spawn` waits for an exec-status pipe to close on `exec`, so it only returned when the child exited, which hung the TUI waiting for a server that was already running. The kernel now tracks a per-process close-on-exec mask (`F_SETFD`, `O_CLOEXEC`, `pipe2`, `F_DUPFD_CLOEXEC`) and `execve` closes those fds.
- The linux terminfo uses SO/SI (`0x0E`/`0x0F`) to switch charsets; the VGA font drew those bytes as `*`-like glyphs all over the screen. The terminal now treats them as charset switches, with a line-drawing map for box characters.
- `sh` defaults `TERM` to `linux` (neovim has that terminfo built in); `bobliu` had no terminfo entry.

## What works and what does not

Verified under QEMU: starting the TUI, insert mode, `Esc`, `:wq`, the saved file's contents, `nvim --version`, `nvim --headless` editing, C filetype detection (statements render bold).

Limits, all expected:

- Startup takes about 90 seconds under QEMU's software emulation because the embedded Lua runtime is parsed from source (no bytecode) and the editor spawns a second `nvim --embed` process. Keys sent faster than the emulated CPU can handle can be merged (for example `Esc` followed at once by `:` becomes Alt-`:`), so pace input when scripting.
- Colours: the console advertises 8 colours, so most highlight groups look plain. `int` and comments are not coloured, `return` is bold.
- The bundled tree-sitter parsers are linked in but neovim looks for them with `dlopen`, so none are registered. Vim-style syntax scripts work; tree-sitter highlighting does not.
- No server socket (`address family not supported`), so `--listen` and RPC over a socket are unavailable. The embedded UI channel uses pipes and works.
- Anything in neovim's runtime that needs LuaJIT's `ffi` or `jit` modules fails, because the Lua here is plain 5.1.
- The runtime is a 3.3 MB subset (Lua modules, filetype detection, a few syntax scripts); help files, spell files, the other ~700 syntax scripts and `ftplugin`/`indent` are not staged.
- `Ctrl-C` kills every user process that does not ignore it, including neovim's server, since bobliu has no process groups.
