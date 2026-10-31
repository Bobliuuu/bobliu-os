# bobliu

A small, from-scratch 32-bit x86 (i686) operating system. It boots from GRUB, sets up its own
descriptor tables, interrupts, paging and memory allocators, mounts a tar-based initrd behind a
tiny VFS, loads ELF binaries into a ring 3 address space, and serves them a three-call syscall
ABI through `int 0x80`. It has a kernel shell so you can poke at all of it.

```
Bobliu's kernel
Kernel shell ready. Type 'help'.
bobliu:/> exec /hello
hello from userland!

bobliu:/>
```

It is a hobby kernel: single process, single core, VGA text mode, PS/2 keyboard. It is built to
learn how the pieces fit together, and it follows the
[OSDev wiki "Creating an Operating System"](https://wiki.osdev.org/Creating_an_Operating_System)
roadmap. The honest status of each phase is below.

## What works

- **Multiboot 1 boot** via GRUB, with memory map and module (initrd) parsing
- **GDT** with ring 0 and ring 3 code/data segments, plus a **TSS** for ring 3 to ring 0 stack switches
- **IDT, ISRs and IRQs**: CPU exceptions 0-31 (reported and halted), remapped 8259 PIC (vectors 32-47), 100 Hz PIT timer, `int 0x80` as a user-callable gate
- **Physical memory manager**: bitmap frame allocator seeded from the multiboot memory map
- **Paging**: identity map of the first 256 MiB, per-process directories cloned from the kernel one, page fault reporter
- **Kernel heap**: bump allocator (`kmalloc`)
- **PS/2 keyboard** driver (US layout, shift and caps lock) feeding a line editor
- **initrd**: a ustar archive loaded as a GRUB module, exposed through a small **VFS** (open/read/close/stat/ls)
- **ELF32 loader** mapping `PT_LOAD` segments into a fresh address space, with a 4-page user stack
- **Syscalls**: `exit`, `write`, `getpid`, numbered like Linux i386
- **A user-space toolchain slice**: `crt0.S`, a linker script, and an `i686-bobliu-gcc` wrapper
- **Kernel shell** with filesystem navigation, memory inspection and `exec`

## Status and roadmap

Mapped to the OSDev wiki phases. Status is based on what is in the tree, not on intent.

### Phase I: kernel foundations

| Item | Status | Notes |
|---|---|---|
| Bare bones | Done | `boot.S`, linker script, VGA terminal, global constructors |
| Multiboot | Done | Multiboot 1; mmap, cmdline (`noshell`), modules |
| GDT | Done | Null, kernel code/data, user code/data, TSS (selector `0x28`) |
| Memory management | Done, basic | Bitmap PMM, identity paging, bump heap with no-op `kfree` |
| Interrupts | Done | Exceptions, remapped PIC, PIT, `int 0x80`; EOI sent first |
| Keyboard | Done | Scancode set 1, US layout, shift/caps, line buffer; no extended keys |
| Filesystem | Done, read-only | tar initrd behind a VFS; no write support, no real disk driver |

### Phase II: user space

| Item | Issue | Status | Notes |
|---|---|---|---|
| User space (ring 3) | #1 | Done | GDT/TSS, `iret` into ring 3, user-accessible pages |
| Program loading | #2 | Done | ELF32 `PT_LOAD` loader reading from the VFS |
| Syscall ABI | #3 | Done | `int 0x80`, Linux-style numbers; `exit`, `write`, `getpid` |
| OS-specific toolchain | #4 | Done, minimal | `crt0.S`, `user.ld`, gcc wrapper; reuses `i686-elf-gcc`, no `i686-bobliu` target |
| libc | #5 | Done | `libc.a` (printf/puts/string, write/getpid/exit via syscalls); no `read`, no `malloc` yet |
| fork / exec | #6 | In progress | Shell-level `exec` works for one process at a time; no `fork`, no scheduler |
| Userland shell | #7 | To do | The shell lives in the kernel; user programs cannot read input yet |

Further out: a scheduler with multiple processes, a freeing heap allocator, a `read` syscall,
a real VFS-backed file descriptor table per process, and a disk driver.

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

`iso.sh` does the following: installs headers into `sysroot/`, builds `libk` and the kernel,
assembles `toolchain/crt0.S`, compiles `initrd/hello.c` with `toolchain/i686-bobliu-gcc`, tars
`initrd/` into `isodir/boot/initrd.tar`, writes a `grub.cfg`, and calls
`i686-elf-grub-mkrescue`. The GRUB menu has two entries: `bobliu (shell)` and
`bobliu (noshell)` (passes `noshell` on the kernel command line).

`qemu.sh` runs with `-no-reboot -no-shutdown -d int,cpu_reset -D qemu.log`, so a triple fault
stops the VM and the interrupt trace lands in `qemu.log`.

Anything you put in `initrd/` ends up in the root of the filesystem. To add a user program,
compile it with the wrapper and drop it there:

```sh
./toolchain/i686-bobliu-gcc -O2 -o initrd/myprog initrd/myprog.c
```

## Shell reference

Commands are split on spaces (no quoting) and the line buffer is 128 characters. Paths may be
absolute or relative to the working directory; `.` and `..` are resolved.

| Command | Description |
|---|---|
| `help` | List commands (the built-in text omits `uptime` and `exec`) |
| `clear` | Clear the screen |
| `echo <text...>` | Print arguments separated by single spaces |
| `ticks` | Raw PIT tick count (100 Hz) |
| `uptime` | Seconds since boot (`ticks / 100`) |
| `mem` | Physical frame totals: total, free, used, in frames and KiB |
| `alloc <bytes>` | Call `kmalloc` and print the pointer and heap usage (leaks by design) |
| `pwd` | Print the working directory |
| `cd [path]` | Change directory (default `/`) |
| `ls [path]` | List a directory |
| `cat <path>` | Print a file |
| `hexdump <path>` | Hex plus ASCII dump, 16 bytes per row |
| `exec <path>` | Load an ELF from the VFS and run it in ring 3 |
| `panic` | Execute `ud2` to trigger an invalid-opcode exception dump |

## Example session

```
bobliu:/> ls
hello
hello.c
hello.txt
readme.txt
setup.txt
dir
bobliu:/> cd dir
bobliu:/dir> ls
note.txt
bobliu:/dir> cat note.txt
cole is hot.
bobliu:/dir> cd /
bobliu:/> mem
frames: total=... free=... used=...
bobliu:/> exec /hello
hello from userland!

bobliu:/>
```

`ls` prints one name per line. The listing shown is illustrative; it and the numbers from `mem` depend on your build and QEMU memory size.

## Repository layout

```
boot / kernel   kernel/arch/i386/   boot.S, linker.ld, cpu/ mm/ dev/ fs/ elf/ tss/ shell/ ...
                kernel/kernel/      kernel.c (kernel_main)
                kernel/include/     kernel headers, including kernel/syscall.h (shared with user code)
libc            libc/               libk (kernel) and libc.a (user space)
toolchain       toolchain/          crt0.S, user.ld, i686-bobliu-gcc, build.sh
initrd          initrd/             files packed into the tar module
scripts         iso.sh qemu.sh build.sh headers.sh config.sh clean.sh
docs            docs/architecture.md
```

## Documentation

[docs/architecture.md](docs/architecture.md) covers the boot flow, memory layout, interrupt
path, the ring 0/ring 3 transition and exec/exit mechanism, the syscall ABI, the VFS, and the
build pipeline, with diagrams, plus a list of known limitations.

## Known limitations

The short version (details in the architecture doc): one user process at a time, no scheduler,
`exec` returns to the shell by resetting the kernel stack rather than unwinding it, the heap
never frees, user pages are placed inside the identity-mapped region, and the user ABI has no
input path yet.
