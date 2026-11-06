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
| utf8proc | Pure C |

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
| 1 | libc headers and kernel syscalls above | A test program exercises each new call under QEMU |
| 2 | libuv builds and runs | A `uvtest` program runs a timer, a pipe pair, a child process and `uv_tty` |
| 3 | Pure-C deps (msgpack, unibilium, libvterm, tree-sitter, lpeg, utf8proc, luv) | Each links into a `deps/` archive |
| 4 | Host codegen | Neovim's generated sources are produced on the host with host `luajit` and cmake |
| 5 | Cross-build `nvim` | `nvim` binary links against the bobliu libc |
| 6 | `nvim --version`, then `nvim --headless -c 'q'` | Exits 0 under QEMU |
| 7 | Interactive editing | Edit, `:w`, `:q`, syntax highlighting with a built-in runtime |

## Risks

- Binary size: the initrd is a tar loaded as a module, so a ~10 MB `nvim` plus runtime files is fine, but the exec loader and the 16-process limit need checking under load.
- Memory: the kernel heap is 4 MiB and user `brk` heap is capped at 256 MiB per process. Neovim allocates heavily at startup.
- No `mmap` file mappings and no sockets: anything in neovim that opens a server socket (`--listen`, RPC over TCP) will not work. Embedded RPC over stdio/pipes should.
- LuaJIT-only features: some runtime Lua uses `ffi`. Those paths need patches or stubs.
