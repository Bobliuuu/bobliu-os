# bobliu

A small, from-scratch 32-bit x86 (i686) operating system. It boots from GRUB, sets up its own
descriptor tables, interrupts, paging and memory allocators, mounts a tar-based initrd behind a
tiny VFS, and runs ELF binaries as preemptively scheduled ring 3 processes with private address
spaces. Processes talk to the kernel through a small syscall ABI on `int 0x80` (`fork`,
`execve`, `waitpid`, `exit`, `read`, `write`, `getpid`, `brk`, plus file calls `open`, `close`,
`lseek`, `stat`, `readdir`, `unlink`, `mkdir`, `dup`, `dup2`), and the default boot drops you into a
small user-space shell that is itself an ELF program. A kernel shell is still there for poking
at the machine.

```
Bobliu's kernel
bobliu sh (pid 1). commands live in /bin; 'exit' leaves.
$ echo hi there
hi there
$ ls /bin
cat  (35340 bytes)
echo  (31500 bytes)
crash  (33100 bytes)
sh  (41196 bytes)
forktest  (37848 bytes)
hello  (32904 bytes)
ls  (35964 bytes)
$ cat /hello.txt
Hello, world!$ 
```

It is a hobby kernel: single core, VGA text mode, PS/2 keyboard, at most 16 processes. It is
built to learn how the pieces fit together, and it follows the
[OSDev wiki "Creating an Operating System"](https://wiki.osdev.org/Creating_an_Operating_System)
roadmap. The honest status of each phase is below.

## What works

- **Multiboot 1 boot** via GRUB, with memory map, command line (`init=`, `noshell`) and module (initrd) parsing
- **GDT** with ring 0 and ring 3 code/data segments, plus a **TSS** whose `esp0` is switched per process
- **IDT, ISRs and IRQs**: CPU exceptions 0-31, remapped 8259 PIC (vectors 32-47), 100 Hz PIT timer, `int 0x80` as a user-callable gate
- **Physical memory manager**: bitmap frame allocator seeded from the multiboot memory map
- **Paging**: the first 256 MiB is identity-mapped and shared by every address space (PDEs 0..63); user space lives at `0x10000000..0x40000000` in per-process page tables; `CR0.WP` is set so read-only pages also bind the kernel
- **Kernel heap**: first-fit free-list allocator with split and coalesce; `kfree` works and the heap's frames are reserved in the PMM
- **Kernel stacks with guard pages**: each process's 16 KiB kernel stack lives in a dedicated region at `0xE0000000`, preceded by an unmapped guard page; overflow prints `KERNEL STACK OVERFLOW` and halts instead of corrupting the heap
- **Processes**: a 16-entry process table, a guarded kernel stack and page directory per process, `swtch()` context switching, a round-robin scheduler, and timer preemption of ring 3 code every 5 ticks (50 ms)
- **fork / exec / waitpid / exit**: `fork` is copy-on-write (frames are shared and write-protected, with a per-frame refcount in the PMM, and copied on the first write), `execve` replaces the image and puts `argc`/`argv` on the new stack, exited processes become zombies until reaped, and a fault in ring 3 kills only that process
- **PS/2 keyboard** driver (US layout, shift and caps lock) feeding a line editor; finished lines go to the kernel shell, or to the console (stdin) buffer while a user program is in the foreground
- **initrd**: a ustar archive loaded as a GRUB module, exposed through a small **VFS** (open/read/write/seek/close/stat/readdir/unlink/mkdir) with a mount table
- **`/tmp`**: a writable in-memory filesystem (ramfs); the initrd itself stays read-only
- **ATA disk driver**: polled PIO on the primary bus, exposed as `/dev/hd0` (whole disk as a byte-addressable file; QEMU gets `disk.img`)
- **ELF32 loader** (`cpu/user_exec.c`) that validates the image, maps `PT_LOAD` segments plus an 8-page stack into a fresh address space, and copies through the identity map
- **File descriptors**: each process has a 16-entry fd table (`PROC_MAXFD`); fds 0-2 are the console, fds 3+ map to VFS files, and they are closed on exit. `fork` and `exec` inherit them (parent and child share the file offset), and `dup`/`dup2` work, including on the console fds, which is what the shell's `>` and `>>` use
- **Syscalls**: `exit`, `fork`, `read`, `write`, `open`, `close`, `waitpid`, `execve`, `unlink`, `getpid`, `lseek`, `mkdir`, `dup`, `brk`, `dup2`, `readdir`, `stat`, numbered like Linux i386
- **A small libc and toolchain slice**: `libc.a` (stdio `printf`/`puts`, string incl. `strcat`, `stdlib` `malloc`/`calloc`/`realloc`/`free` on top of `sbrk`, `fcntl.h`, `unistd` incl. `open`/`close`/`lseek`/`dup`/`unlink`, `sys/stat.h` `stat`, `dirent.h` `readdir_at`, `sys/wait.h`, `exit`/`abort`), `crt0.S`, a linker script and an `i686-bobliu-gcc` wrapper
- **User programs** in `user/` (`sh`, `hello`, `echo`, `cat`, `ls`, `cp`, `rm`, `mkdir`, `hd`, and the tests `forktest`, `fdtest`, `memtest`, `wtest`, `pipetest`, `sigtest`, `timetest`, `crash`), plus `wc`, `grep`, `date`, `sleep`, `vi` built into `initrd/bin`
- **Kernel shell** with filesystem navigation, memory inspection, `ps` and `exec`

## Status and roadmap

Mapped to the OSDev wiki phases. Status is based on what is in the tree, not on intent.

### Phase I: kernel foundations

| Item | Status | Notes |
|---|---|---|
| Bare bones | Done | `boot.S`, linker script, VGA terminal, global constructors |
| Multiboot | Done | Multiboot 1; mmap, cmdline (`noshell`), modules |
| GDT | Done | Null, kernel code/data, user code/data, TSS (selector `0x28`) |
| Memory management | Done, basic | Bitmap PMM, identity paging for the kernel, free-list heap with a working `kfree` |
| Interrupts | Done | Exceptions, remapped PIC, PIT, `int 0x80`; EOI sent first |
| Keyboard | Done | Scancode set 1, US layout, shift/caps, line buffer; no extended keys; lines routed to the shell or to stdin |
| Filesystem | Done | tar initrd (read-only) and a ramfs on `/tmp` behind a VFS with a mount table, reachable from user space through file syscalls; `/dev/hd0` for the ATA disk; no on-disk filesystem yet |

### Phase II: user space

| Item | Issue | Status | Notes |
|---|---|---|---|
| User space (ring 3) | #1 | Done | GDT/TSS, `iret` into ring 3, per-process page tables at `0x10000000..0x40000000` |
| Program loading | #2 | Done | ELF32 loader in `cpu/user_exec.c`: validates headers, copies via the identity map |
| Syscall ABI | #3 | Done | `int 0x80`, Linux-style numbers; `exit`, `fork`, `read`, `write`, `waitpid`, `execve`, `getpid` (file syscalls added in #8) |
| OS-specific toolchain | #4 | Done, minimal | `crt0.S` (passes `argc`/`argv`), `user.ld`, gcc wrapper; reuses `i686-elf-gcc`, no `i686-bobliu` target |
| libc | #5 | Done | `libc.a`: stdio (`printf`, `puts`), string (incl. `strchr`, `strcat`), `unistd`, `sys/stat.h`, `dirent.h`, `sys/wait.h`, `exit`/`abort`; `malloc` since #11 |
| fork / exec | #6 | Done | Process table, round-robin scheduler, `swtch`, `fork` (copy-on-write since #9), `execve`, `waitpid`, zombies |
| Userland shell | #7 | Done | `user/sh.c`: reads a line, `fork` + `execv` from `/bin`, `waitpid` |

| File syscalls | #8 | Done (writes came in #12) | Per-process fd table (16 entries); `open`, `close`, `lseek`, `stat`, `readdir`, and `read` on fds 3+; `user/cat.c` and `user/ls.c` |
| Memory hardening | #9 | Done | Kernel-stack guard pages (region at `0xE0000000`) and copy-on-write `fork` (`P_COW` bit, per-frame refcounts, `CR0.WP`) |

| fd inheritance, `dup` | #10 | Done | fds are shared across `fork`/`exec` (refcounted open files), `dup`, `dup2` |
| `brk` and `malloc` | #11 | Done | heap at `0x20000000`, copy-on-write across `fork`; first-fit `malloc` with coalescing in libc |
| Writable files | #12 | Done | ramfs mounted on `/tmp`; `open` flags (`O_CREAT`, `O_TRUNC`, `O_APPEND`), `write`, `unlink`, `mkdir`; shell `>` and `>>`; `cp`, `rm`, `mkdir` |
| Disk driver | #13 | Done | ATA PIO (LBA28) on the primary master, `/dev/hd0`; `hd` dumps it, `echo x > /dev/hd0` writes it |

Phase II is complete (#1-#13). Follow-ups that closed the gaps it left:

| Item | Issue | Notes |
|---|---|---|
| Working directory | #14 | `chdir`/`getcwd`, relative paths resolved in the kernel against a per-process cwd (inherited by `fork`/`exec`); `cd`, `pwd` in `sh` |
| Pipes | #15 | `pipe` syscall (4 KiB ring buffer, blocking, EOF and broken-pipe handling); `a \| b \| c` in `sh`; `wc`, `grep` |
| Signals | #16 | `kill`, `signal`, `sigreturn`; handlers run on the user stack, a signal interrupts blocking `read`/`waitpid`/pipe calls; Ctrl-C sends `SIGINT` to every user process that isn't ignoring it (the shell ignores it); no process groups |
| On-disk filesystem | #17 | `bobfs` (1 KiB blocks, 128 inodes, 12 direct + 1 indirect block, write-through), mounted on `/disk`; run `mkfs` in the kernel shell once to format `disk.img` |
| Lite vi | #18 | `/bin/vi`: raw console mode (`tcsetraw`), a small ANSI subset in the VGA terminal, normal/insert/`:` modes |

### Phase III: extending the OS

| Item | Issue | Status | Notes |
|---|---|---|---|
| Time | #20 | Done | CMOS RTC read at boot, wall clock = boot epoch + PIT ticks (100 Hz); `time`, `clock_gettime`, `nanosleep`; `sleep`, `usleep`, `gmtime_r`; `date`, `sleep` programs |
| Threads | #21 | Open | |
| Secondary storage | #13 | Done | ATA PIO driver |
| Real filesystems | #17 | Done (one) | `bobfs`; no FAT/ext2 |
| Graphics, UI, networking, sound, USB | #19 (doom) | Open | doom needs a framebuffer, raw key up/down events and time first |


## Build and run

You need:

- an `i686-elf` cross toolchain (binutils and GCC) in `~/opt/cross/bin`
- `i686-elf-grub-mkrescue` (GRUB built for `i686-elf`)
- `xorriso`
- `qemu-system-i386`
- GNU `make` (the scripts call `gmake` by default; override with `MAKE=make`)

```sh
export PATH="$HOME/opt/cross/bin:$PATH"

./iso.sh      # builds libc/kernel, user programs, initrd, and bobliu.iso
./qemu.sh     # runs iso.sh, then boots bobliu.iso in QEMU
./clean.sh    # remove build products
```

`iso.sh` does the following: installs headers into `sysroot/`, builds `libc.a`, `libk.a` and the
kernel, assembles `toolchain/crt0.S`, compiles every `user/*.c` with `toolchain/i686-bobliu-gcc`
into `initrd/bin/` (gitignored), tars `initrd/` into `isodir/boot/initrd.tar`, writes a
`grub.cfg`, and calls `i686-elf-grub-mkrescue`. The GRUB menu has three entries:

| Entry | Kernel command line | Behavior |
|---|---|---|
| `bobliu (user shell)` (default) | `init=/bin/sh` | spawns `/bin/sh` as the first process; when it exits you fall back to the kernel shell |
| `bobliu (kernel shell)` | (none) | kernel shell only |
| `bobliu (noshell)` | `noshell` | no shell and keyboard input is ignored |

`qemu.sh` runs with `-no-reboot -no-shutdown -d int,cpu_reset -D qemu.log`, so a triple fault
stops the VM and the interrupt trace lands in `qemu.log`.

Anything you put in `initrd/` ends up in the root of the filesystem. To add a user program,
drop a `.c` file in `user/` and re-run `./iso.sh`; it shows up as `/bin/<name>`. To build one by
hand:

```sh
./toolchain/i686-bobliu-gcc -O2 -o initrd/bin/myprog user/myprog.c
```

## User programs

| Program | What it does |
|---|---|
| `vi` | Lite vi editor (`vi <file>`): NORMAL/INSERT/`:` modes, h j k l and arrows, `0 $ gg G x dd o O i a A I u`, `:w :q :q! :wq :x`. Uses `tcsetraw` (raw console) and the ANSI subset of the VGA terminal |
| `sh` | The user shell: prompt `$ `, splits on spaces (max 16 args), runs `/bin/<cmd>` (or a path containing `/`) via `fork` + `execv` + `waitpid`. Prints `sh: <cmd>: not found` if `execv` fails and `[exit N]` for a nonzero status. Supports `> file` and `>> file` redirection of stdout (as separate words). Built-in: `exit` |
| `hello` | Prints a greeting, its pid and some libc output |
| `echo` | Prints its arguments |
| `cat` | `cat <file>...`: opens each file with `open`, copies it to stdout, prints `cat: <file>: cannot open` on failure |
| `ls` | `ls [dir]` (default `/`): lists a directory with `readdir_at`, showing `name/` for directories and `name  (N bytes)` for files |
| `forktest` | `fork` + `waitpid` exit status, `fork` + `execv`, then 40 short-lived children to check that memory comes back |
| `cp`, `rm`, `mkdir` | `cp <src> <dst>`, `rm <path>...` (files and empty dirs), `mkdir <path>...`; writes only work on `/tmp` (and `/dev/hd0`) |
| `hd` | `hd <file> [offset] [len]`: hex dump, e.g. `hd /dev/hd0 0 64` |
| `fdtest` | `dup`/`dup2`, shared offsets across `fork`, closing stdout and restoring it, and no fd leak over 40 forks |
| `memtest` | `sbrk`/`brk`, `malloc`/`free` reuse and coalescing, `calloc`/`realloc`, copy-on-write heap after `fork`, children that grow the heap and exit |
| `wtest` | Create/write/append/truncate on `/tmp`, initrd stays read-only, `mkdir`/`unlink`, unlink-while-open, shared fd writes across `fork`, create/delete churn |
| `crash` | Checks that bad syscall arguments are rejected, a failed `execv` returns, and a NULL write kills only this process |

## Shell reference

There are two shells.

**User shell** (`/bin/sh`, default GRUB entry): see the table above. Type `exit` to leave it,
after which the kernel prints `[init exited]` and the kernel shell starts.

**Kernel shell**: runs from the kernel main loop (`shell_poll`), not inside the keyboard IRQ, so
commands such as `exec` can block. Commands are split on spaces (no quoting, up to 16 tokens)
and the line buffer is 128 characters. Paths may be absolute or relative to the working
directory; `.` and `..` are resolved. While the kernel shell is waiting for a program started
with `exec`, keyboard lines go to that program's stdin instead.

| Command | Description |
|---|---|
| `help` | List commands (the built-in text omits `uptime`) |
| `clear` | Clear the screen |
| `echo <text...>` | Print arguments separated by single spaces |
| `ticks` | Raw PIT tick count (100 Hz) |
| `uptime` | Seconds since boot (`ticks / 100`) |
| `mem` | Physical frame totals: total, free, used, in frames and KiB |
| `alloc <bytes>` | Call `kmalloc` and print the pointer and heap usage (the block is never freed) |
| `pwd` | Print the working directory |
| `cd [path]` | Change directory (default `/`) |
| `ls [path]` | List a directory |
| `cat <path>` | Print a file |
| `hexdump <path>` | Hex plus ASCII dump, 16 bytes per row |
| `exec <path> [args...]` | Load an ELF from the VFS, run it as a process with the given arguments, and wait for it (prints `[pid N exited with S]` for a nonzero status) |
| `ps` | List processes: pid, parent pid, state, name |
| `panic` | Execute `ud2` to trigger an invalid-opcode exception dump |

## Example session

Booting the default "user shell" entry:

```
Bobliu's kernel
bobliu sh (pid 1). commands live in /bin; 'exit' leaves.
$ echo hello world
hello world
$ hello
hello from userland!
pid=3 strlen=6 hex=beef str=libc works
$ forktest
parent pid=4
child  pid=5 (fork returned 0)
parent: reaped 5, exit status 7
cow isolation: ok
hello from userland!
pid=6 strlen=6 hex=beef str=libc works
parent: exec child done, status 0
parent-writes-first: ok
40 forks ok
$ ls /bin
cat  (35340 bytes)
echo  (31500 bytes)
crash  (33100 bytes)
sh  (41196 bytes)
forktest  (37848 bytes)
hello  (32904 bytes)
ls  (35964 bytes)
$ cat /hello.txt
Hello, world!$ nope
sh: nope: not found
[exit 127]
$ exit
[init exited]
Kernel shell ready. Type 'help'.
bobliu:/> ps
PID PPID STATE    NAME
0   0    running  kernel
bobliu:/> exec /bin/echo back in the kernel shell
back in the kernel shell
bobliu:/>
```

The exact pids, the middle of the `forktest` output and the listings depend on your build;
pids are never reused within a boot.

## Repository layout

```
boot / kernel   kernel/arch/i386/   boot.S, swtch.S, isr_stubs.S, linker.ld,
                                    cpu/ (gdt idt isr proc user_exec) mm/ (pmm paging heap syscall)
                                    dev/ (console keyboard timer pic vga) fs/ shell/ tss/ interrupts/ ...
                kernel/kernel/      kernel.c (kernel_main)
                kernel/include/     kernel headers; kernel/syscall.h is shared with user code
libc            libc/               libk.a (kernel) and libc.a (user space), headers in libc/include
user programs   user/               sh, hello, echo, cat, ls, forktest, crash (built into initrd/bin)
toolchain       toolchain/          crt0.S, user.ld, i686-bobliu-gcc, build.sh
initrd          initrd/             files packed into the tar module (initrd/bin is generated)
scripts         iso.sh qemu.sh build.sh headers.sh config.sh clean.sh
docs            docs/architecture.md
```

## Documentation

[docs/architecture.md](docs/architecture.md) covers the boot flow, memory layout, interrupt
path, the process model (copy-on-write fork, exec/wait/exit and the scheduler), the syscall ABI, the VFS and per-process fd table, the
build pipeline and the shells, with diagrams, plus a list of known limitations.

## Known limitations

The short version (details in the architecture doc): `/tmp` (RAM, lost at reboot, 256 KiB
per file), `/disk` (bobfs, 268 KiB per file, 128 files, 27-character names, no cache or journal) and `/dev/hd0` are writable, there are no
process groups or threads, user
processes have no working directory (paths are absolute), at most 16 processes, 16 fds each, and
16 arguments of 128 bytes, `envp` is ignored, stdin is one global console buffer, blocking
happens only in `waitpid` and `read`, the pid 0 boot stack has no guard page, kernel-mode
exceptions halt the machine, an out-of-memory during a copy-on-write fault kills the process,
and some dead code (for example `kernel/arch/i386/elf/elf.c`) is still compiled in.
