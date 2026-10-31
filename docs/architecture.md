# bobliu architecture

This document describes how the kernel is put together, as implemented in the tree. Source paths
are relative to `kernel/arch/i386/` unless noted. Syscalls and the shells are covered here
(sections 8 and 10) rather than in separate files.

Contents: [1 Boot flow](#1-boot-flow) | [2 Layers](#2-layered-components) |
[3 Memory](#3-memory-layout) | [4 Interrupts](#4-interrupt-and-irq-flow) |
[5 Processes and scheduling](#5-processes-and-scheduling) | [6 TSS/GDT](#6-gdt-and-tss) |
[7 initrd and VFS](#7-initrd-and-vfs) | [8 Syscall ABI](#8-syscall-abi) |
[9 Toolchain and build](#9-toolchain-and-build-pipeline) | [10 Shells and console](#10-shells-and-console) |
[11 Known limitations](#11-known-limitations)

## 1. Boot flow

GRUB loads `/boot/myos.kernel` (Multiboot 1 header requests page-aligned modules and a memory
map) and the initrd as a module named `initrd`. `_start` in `boot.S` sets a 16 KiB boot stack and
calls into C. That boot stack becomes the stack of the kernel context (pid 0) for the rest of
the run.

```mermaid
flowchart TD
    G[GRUB<br/>multiboot /boot/myos.kernel [init=/bin/sh]<br/>module /boot/initrd.tar initrd] --> S["_start (boot.S)<br/>esp = 16 KiB boot stack<br/>save eax=magic, ebx=mbi"]
    S --> E["kernel_early(magic, mbi)<br/>VGA marker, stack-guard value,<br/>terminal_initialize()"]
    E --> C["call_global_constructors()<br/>.ctors walked backwards"]
    C --> K[kernel_main]
    K --> K1[printf banner]
    K1 --> K2[multiboot1_init + cmdline]
    K2 --> K3["interrupts_init()<br/>gdt_init, idt_init, pic_remap(0x20,0x28),<br/>unmask IRQ0/1, PIT 100 Hz, keyboard, sti"]
    K3 --> K4["pmm_init()<br/>bitmap from mmap"]
    K4 --> K5["paging_init_identity()<br/>0..256 MiB, enable CR0.PG"]
    K5 --> K6["heap_init(0x800000, 0x400000)<br/>proc_init() -> pid 0 = this context"]
    K6 --> K7["gdt_init, tss_init,<br/>tss_set_kernel_stack(static 4 KiB kstack),<br/>tss_flush (ltr 0x28), idt_init"]
    K7 --> K8["find module 'initrd'<br/>initrd_vfs_init, vfs_init(root)"]
    K8 --> K9["keyboard_enable_shell(!noshell)"]
    K9 --> K10{"init=PATH on cmdline<br/>and shell enabled?"}
    K10 -- yes --> SP["proc_spawn(PATH), proc_wait(pid)<br/>(kernel context sleeps until it exits)<br/>print [init exited]"]
    K10 -- no --> SI
    SP --> SI["shell_init (unless noshell)"]
    SI --> L["main loop (pid 0):<br/>shell_poll(); proc_reap_orphans(); sti; hlt"]
```

Order of operations in `kernel/kernel/kernel.c`, exactly:

1. `printf("Bobliu's kernel")`
2. `multiboot1_init`, `multiboot1_cmdline`
3. `interrupts_init()` (this also does `gdt_init` and `idt_init` and executes `sti`)
4. `pmm_init`, then two throwaway `pmm_alloc_frame` calls that are freed after the heap is up
5. `paging_init_identity()`
6. `heap_init(0x00800000, 0x00400000)`, then `proc_init()`
7. `gdt_init`, `tss_init`, `tss_set_kernel_stack(kstack)`, `tss_flush`, `idt_init` (second time for GDT/IDT)
8. `multiboot1_dump_modules`, then initrd/VFS setup if a module whose string is `initrd` exists
9. `keyboard_enable_shell(true)` unless `noshell` is on the command line
10. If the command line has `init=<path>` and the shell is enabled: `proc_spawn(path)` and `proc_wait` on it, then print `[init exited]` (or `init: cannot run <path>`)
11. `shell_init` unless `noshell`, then the main loop (`shell_poll`, `proc_reap_orphans`, `sti; hlt`)

Interrupts are therefore enabled before the PMM and paging exist. This works because only the
timer and keyboard are unmasked and their handlers do not allocate memory; the timer handler's
preemption only fires for ring 3 code, which cannot exist yet.

## 2. Layered components

```mermaid
flowchart TB
    subgraph U["Ring 3 (one address space per process)"]
        UP["user programs (ELF32)<br/>crt0 -> main(argc, argv) -> exit<br/>/bin/sh, hello, echo, ..."]
    end
    subgraph K["Ring 0"]
        direction TB
        SH["kernel shell (shell/shell.c)<br/>runs in the pid 0 main loop"]
        PR["process layer<br/>cpu/proc.c, swtch.S: table, sched, fork/exec/wait/exit"]
        EX["ELF loader<br/>cpu/user_exec.c, elf.h"]
        SC["syscall_handle (mm/syscall.c)"]
        VFS["VFS (fs/vfs.c)"]
        IRD["initrd vnode ops (fs/initrd_vfs.c)<br/>tar reader (fs/initrd_tar.c)"]
        MM["pmm / paging / heap"]
        INT["isr_stubs.S -> isr.c<br/>irq_handler, page_fault"]
        CON["console stdin buffer<br/>dev/console.c"]
        DEV["drivers: tty/VGA, PS/2 keyboard,<br/>PIT timer, 8259 PIC"]
        CPU["GDT, IDT, TSS"]
    end
    HW[("Hardware: VGA text, i8042, PIT, PIC, RAM")]
    UP -- "int 0x80" --> INT
    INT --> SC
    SC --> PR
    SC --> CON
    PR --> EX
    PR --> MM
    EX --> VFS
    EX --> MM
    VFS --> IRD
    INT -- "IRQ0 timer: preempt ring 3" --> PR
    INT -- IRQ1 --> DEV
    DEV -- "line + Enter:<br/>shell_submit" --> SH
    DEV -- "line, if pid 0 is<br/>waiting on a child" --> CON
    SH --> PR
    SH --> VFS
    SC --> DEV
    MM --> HW
    DEV --> HW
    CPU --- INT
```

The kernel shell is not a user process: it runs in the kernel main loop (pid 0), outside any
IRQ handler. `libk` (`libc/`) provides `printf`, `string.h` and friends to the kernel;
`libc.a` is the same code plus syscall wrappers for user space.

## 3. Memory layout

### Physical memory

Managed by a bitmap allocator (`mm/pmm.c`), one bit per 4 KiB frame, first-fit scan.

```
0x00000000 +--------------------------+
           | low 1 MiB (always used)  |  BIOS, VGA at 0xB8000, etc.
0x00100000 +--------------------------+  <- __kernel_start / load address (1 MiB)
           | .text .rodata .data      |
           | .init_array .ctors .bss  |
           | __kernel_end             |
           +--------------------------+
           | PMM bitmap (16-aligned)  |  placed right after __kernel_end
           +--------------------------+
           | multiboot modules        |  marked used (initrd.tar)
           |   (GRUB places them)     |
           +--------------------------+
           | kernel heap 0x800000..   |  reserved in the PMM by heap_init
           |            0xC00000      |
           +--------------------------+
           | free frames (mmap type 1)|  page directories/tables, user pages
           +--------------------------+
```

The PMM starts with every frame marked used, frees all `type == 1` mmap regions, then re-marks
the first 1 MiB, the kernel image, the bitmap and all modules as used. `heap_init` reserves its
4 MiB range the same way, so the PMM never hands those frames out. Total frames come from the
highest end address of available RAM in the memory map.

### Virtual address space

Paging is a 2-level 32-bit scheme with 4 KiB pages. The kernel identity-maps 0..256 MiB
(`KERNEL_ID_MAP_MB = 256`, PDEs 0..63). Every process page directory copies those 64 PDEs from
the kernel directory, so they point at the **same page tables** everywhere and kernel memory is
visible (supervisor-only) in every address space. User space is the range
`USER_LO..USER_HI = 0x10000000..0x40000000` (PDEs 64..255), whose page tables are private to
each process. Everything else is unmapped.

```
0x00000000 +----------------------------------+
           | kernel + low memory (identity)   |  shared PDEs 0..63, supervisor only
0x00100000 | kernel image, PMM bitmap, ...    |
0x00800000 +----------------------------------+ <- kernel heap (heap_init)
           | kernel heap, 4 MiB               |    free list; also holds every process's
0x00C00000 +----------------------------------+    16 KiB kernel stack
           | ...                              |
0x00FF0000 |  PF_TMP_PT_VA (kernel page fault |
           |   walker, debug only)            |
           | ... frames for page tables and   |
           |     user pages (identity mapped) |
0x10000000 +----------------------------------+ <- 256 MiB: end of identity map, USER_LO
           | user .text .rodata .data .bss    |    ELF load base (user.ld: . = 0x10000000)
           | (grows up; private page tables)  |
           |              ...                 |
           |  unmapped (holes fault)          |
0x3FFF8000 +----------------------------------+ <- user stack: USER_STACK_PAGES = 8, grows down
           | user stack (argc/argv at the top)|
0x40000000 +----------------------------------+ <- USER_HI / USER_STACK_TOP
           | unmapped                         |
0xFFFFFFFF +----------------------------------+
```

How this behaves:

- A user page is a freshly allocated PMM frame mapped at a user virtual address. For ring 3
  access both the PDE and the PTE need `P_USER`; `get_or_alloc_pt_in` sets it on the PDE when a
  user page is mapped. Kernel PDEs stay supervisor-only.
- The kernel reaches physical frames (page tables, user pages being loaded or copied) through
  the identity map, which is why page tables and user frames are allocated from PMM frames below
  256 MiB in practice (the PMM does not enforce a cap; see limitations).
- Loading and `fork` copy into a not-yet-loaded address space by translating each user address
  to its physical frame and writing through the identity map, so no CR3 switching is needed.
- All `PT_LOAD` segments are mapped `P_PRESENT | P_USER | P_RW` regardless of `PF_W`.
- `user_range_ok` (`mm/syscall.c`) accepts a user buffer only if it lies inside
  `[USER_LO, USER_HI)` and every page it touches is mapped in the caller's directory.
- `paging_free_user` walks PDEs 64..1023 of a directory, frees every mapped user frame and each
  page table, and optionally the directory frame.

### Heap

`heap_init(0x00800000, 0x00400000)` sets up a first-fit free-list allocator over
`0x800000..0xC00000`. Blocks carry a 16-byte header (size, next, free flag, magic) and payloads
are 16-byte aligned. `kmalloc` splits a large free block; `kfree` marks a block free and
coalesces with the following and preceding free blocks (a bad or double free prints a message
and is ignored). The heap range is identity mapped and its frames are reserved in the PMM.
Heap users: per-process kernel stacks (16 KiB each) and the temporary ELF image buffer.

## 4. Interrupt and IRQ flow

Vectors:

| Vectors | Source | Stub |
|---|---|---|
| 0-31 | CPU exceptions (8, 10-14, 17, 30 push an error code; others get a dummy 0) | `isr0..isr31` |
| 32-47 | 8259 PIC IRQ0-15, remapped by `pic_remap(0x20, 0x28)` | `irq0..irq15` |
| 0x80 | software interrupt, DPL 3 gate (`0xEE`) | `isr128` |

All other gates are `0x8E` (ring 0 interrupt gates). Only IRQ0 (timer, 100 Hz) and IRQ1
(keyboard) are unmasked.

```mermaid
sequenceDiagram
    participant HW as PIC / CPU
    participant ST as isr_stubs.S
    participant IRQ as irq_handler (isr.c)
    participant H as irq_routines[n]
    HW->>ST: vector 32+n (IRQ) pushes eflags/cs/eip<br/>(from ring 3: also ss/esp, stack = TSS esp0)
    ST->>ST: cli, push err=0, push vector
    ST->>ST: pusha, save DS, load 0x10 into ds/es/fs/gs
    ST->>IRQ: irq_handler(regs*)
    IRQ->>HW: pic_send_eoi (slave 0xA0 if >= 0x28, then master 0x20)
    IRQ->>H: call registered handler (timer_cb / keyboard_irq)
    H-->>IRQ: return (timer_cb may switch to another process first)
    IRQ-->>ST: ret
    ST->>ST: restore DS..GS, popa, drop vector+err, sti
    ST->>HW: iret
```

**EOI is sent first.** The timer handler may call `proc_yield()` and switch to another process,
and the interrupted one may not run again for a while. If the EOI were sent last, the PIC would
stay "in service" for IRQ0 and the timer would stop. `irq_handler` has a comment saying this.

The timer (`dev/timer.c`) increments the tick count and, only when the interrupted code was in
ring 3 (`cs & 3`) and `ticks % 5 == 0`, calls `proc_yield()`. Ring 0 code is never preempted.

Exceptions go through `isr_common` to `isr_handler(regs_t*)`:

- vector 0x80: `isr128_handler` path -> `syscall_handle`
- vector 14: `page_fault_handler`. From ring 3 it prints `[pid N killed: segfault at ADDR, eip=...]` and calls `proc_exit(139)`. From ring 0 it prints CR2, EIP, error bits and walks the page tables, then halts (`cli; hlt` loop)
- other vectors below 32: from ring 3, prints `[pid N killed: <name> at eip=...]` and calls `proc_exit(128 + vector)`; from ring 0, prints the name, EIP/CS/EFLAGS and registers, then halts

## 5. Processes and scheduling

### Data structures

`kernel/include/kernel/proc.h`, implemented in `cpu/proc.c` and `swtch.S`.

- A static table of `NPROC = 16` `proc_t` entries. Slot 0 is the **kernel context** (pid 0): it
  has no private kernel stack (it runs on the boot stack) and uses the kernel page directory.
  It is the parent that adopts orphans and the process that runs the kernel shell.
- States: `UNUSED`, `RUNNABLE`, `RUNNING`, `SLEEPING` (blocked on a channel: `WAIT_CHILD` or
  `WAIT_CONSOLE`), `ZOMBIE`.
- Each other process owns a **kernel stack** (`KSTACK_SIZE = 16384` bytes from `kmalloc`) and a
  **page directory** (kernel PDEs shared, user PDEs private). Pids come from a counter and are
  never reused within a boot; the shell started by `init=` is pid 1.
- `struct context` holds the callee-saved registers (`edi, esi, ebx, ebp`) plus the return `eip`.
  `swtch(&old->ctx, new->ctx)` pushes those on the old stack, saves `esp`, loads the new `esp`,
  pops and returns.
- A new process's kernel stack is laid out from the top as `[trap frame (regs_t)]` then
  `[context]` below it, with the context's `eip` set to `trapret`.

### Scheduler

`sched()` is round robin over the table starting after the current slot. It disables interrupts,
picks the next `RUNNABLE` entry, sets the TSS `esp0` to the next process's kernel stack top (if
it has one), loads its CR3, and calls `swtch`. If nothing is runnable and the caller is not
itself running, it executes `sti; hlt; cli` until an interrupt makes something runnable.

```mermaid
sequenceDiagram
    participant A as process A (ring 3)
    participant T as timer IRQ0 (A's kernel stack)
    participant S as sched()
    participant W as swtch.S
    participant B as process B
    A->>T: tick 5, 10, ... interrupts ring 3 code
    T->>T: EOI first, ticks++, cs & 3 so proc_yield()
    T->>S: A.state RUNNABLE
    S->>S: pick_next() = B (round robin)
    S->>S: tss.esp0 = B.kstack top, load B.pd into CR3
    S->>W: swtch(&A.ctx, B.ctx)
    W->>W: push edi esi ebx ebp, save esp in A.ctx<br/>load B.ctx, pop regs, ret
    alt B has run before
        W->>B: ret resumes B inside its own sched() call
    else first run
        W->>B: ret lands in trapret: pop ds, popa, drop vector/err, iret to ring 3
    end
    Note over A,B: later, somebody switches back to A and it<br/>returns from swtch, leaves the IRQ and iret's to user code
```

Processes that block (`waitpid`, `read` on an empty console) set their state to `SLEEPING` with a
channel and call `sched()` directly from inside the syscall. A wakeup (`proc_wakeup(chan)` from
the console on a new line, or `proc_exit` for the parent's `WAIT_CHILD`) makes them `RUNNABLE`.

### fork

```mermaid
sequenceDiagram
    participant P as parent (ring 3)
    participant K as syscall_handle / proc_fork
    participant PG as paging_clone_user
    participant C as child (new slot)
    P->>K: int 0x80, eax=2 (SYS_fork)
    K->>K: proc_alloc: slot, kmalloc 16 KiB kstack,<br/>pid, parent = current
    K->>PG: new directory: copy kernel PDEs, then for every user PTE<br/>alloc a frame and copy the 4 KiB (no COW)
    K->>C: copy parent's trap frame onto child's kstack, child eax = 0
    K->>C: state = RUNNABLE
    K-->>P: eax = child pid, iret
    Note over C: when scheduled, swtch returns into trapret,<br/>iret: child continues after int 0x80 with fork() == 0
```

On out-of-memory (no slot, no kernel stack, or no frames) `fork` returns -1 and releases what it
allocated.

### exec

`execve(path, argv, envp)` replaces the calling process's image in place. `syscall_handle`
copies the path and up to `PROC_MAX_ARGS = 16` arguments of at most `PROC_ARG_LEN = 128` bytes
out of user space (each pointer and byte validated with `user_range_ok`); `envp` is not read. If
`argv` is NULL, `argv[0]` is the path. Then `proc_exec`:

1. `user_load` builds a **new** address space (below). On failure nothing has changed and the
   syscall returns -1 to the old program.
2. Switches CR3 to the new directory and frees every user frame and page table of the old one.
3. Rewrites the existing trap frame: `eip` = ELF entry, `useresp` = prepared stack pointer,
   segments set to the ring 3 selectors, `eflags` with IF set. The syscall's `iret` then lands
   in the new program. `eax` stays 0.

`user_load` (`cpu/user_exec.c`):

- `vfs_stat` the path (must be a file, at most 512 KiB), read the whole file into a temporary
  `kmalloc` buffer, freed afterwards.
- Validate: ELF magic, ELF32, little-endian, `ET_EXEC`, `EM_386`, program header size, and that
  the program header table fits in the file. Each `PT_LOAD` must satisfy
  `p_vaddr >= USER_LO`, end at or below `USER_HI` minus the stack, `p_filesz <= p_memsz`, and
  file range inside the image.
- `paging_clone_directory(kernel dir)` for a fresh directory; for each `PT_LOAD` page allocate a
  zeroed frame, map it `P_PRESENT | P_RW | P_USER`, and copy the file bytes through the identity
  map (the BSS tail is already zero). Map 8 stack pages ending at `USER_STACK_TOP`.
- Build the initial stack: argument strings at the top, 16-byte alignment, then
  `argv[0..n-1]`, a NULL, and `argc` below that. The returned stack pointer points at `argc`.
- The entry point must be inside the user range. On any failure the partly built address space is
  freed.

`crt0.S` (`_start`) reads `argc` and `argv` from the stack, calls `main(argc, argv)`, and issues
`SYS_exit` with the return value in `ebx`.

`proc_spawn(path, argv, argc)` (used by the kernel for `init=` and the `exec` command) does the
same `user_load`, then allocates a slot and builds a fresh trap frame (`cs = 0x1B`, `ss/ds =
0x23`, `eflags = 0x202`) for it to `iret` into, and marks it `RUNNABLE`. Its parent is the
current process, i.e. pid 0.

### wait and exit

```mermaid
sequenceDiagram
    participant P as parent
    participant C as child
    participant S as sched()
    participant K as kernel context (pid 0)
    P->>P: waitpid(pid): int 0x80, eax=7
    P->>P: proc_wait: scan table for matching child
    alt child already ZOMBIE
        P->>P: status = (code & 0xFF) << 8, proc_release (free kstack, slot)
    else child still running
        P->>S: state SLEEPING, chan WAIT_CHILD, sched()
        C->>C: exit(n): int 0x80, eax=1 (or killed by a ring 3 fault)
        C->>C: switch CR3 to kernel dir, free all user frames,<br/>page tables and the directory
        C->>K: reparent C's own children to pid 0
        C->>C: exit_code = n, state ZOMBIE
        C->>P: parent SLEEPING on WAIT_CHILD -> RUNNABLE
        C->>S: sched(), never returns
        S->>P: swtch into parent, wait loop rescans, reaps zombie
    end
    P-->>P: eax = child pid, *status written (if user pointer valid)
```

Details:

- A zombie keeps its slot and kernel stack (it is still running on that stack when it calls
  `sched()` the last time) until its parent reaps it in `proc_wait`; `proc_release` then
  `kfree`s the stack.
- `waitpid(-1, ...)` (also `wait`) accepts any child; `waitpid(pid, ...)` a specific one; both
  return -1 immediately when there is no matching child. The `options` argument is ignored. The
  status is `(exit_code & 0xFF) << 8`, which is what `WEXITSTATUS` in `sys/wait.h` undoes.
- A child that dies from a CPU exception or page fault in ring 3 exits with `128 + vector`
  (`139` for a page fault); nothing else about the other processes is affected.
- Orphans are adopted by pid 0. The kernel main loop calls `proc_reap_orphans()` on every wake-up
  to release zombies whose parent is pid 0 and that nobody waited for.
- `proc_kernel_busy()` reports whether pid 0 is `SLEEPING`. The keyboard path uses it to decide
  where a finished input line goes (section 10).

## 6. GDT and TSS

| Index | Selector | Descriptor |
|---|---|---|
| 0 | 0x00 | null |
| 1 | 0x08 | kernel code, ring 0, base 0, limit 4 GiB (`0x9A`, `0xCF`) |
| 2 | 0x10 | kernel data, ring 0 (`0x92`) |
| 3 | 0x18 | user code, ring 3 (`0xFA`); used as `0x1B` with RPL 3 |
| 4 | 0x20 | user data, ring 3 (`0xF2`); used as `0x23` |
| 5 | 0x28 | 32-bit available TSS (`0x89`), `ss0 = 0x10`, `esp0` set per process, no I/O bitmap |

`tss_flush` executes `ltr` with `0x28`. `esp0` is initially the top of a static 4 KiB `kstack`
in `kernel.c` and is updated by `sched()` to the top of the next process's kernel stack on every
switch to a process that has one. The ring 0 segments are flat; protection between user and
kernel, and between processes, is done by paging (`P_USER` bits and private user page tables).

## 7. initrd and VFS

```mermaid
flowchart LR
    GRUB["GRUB module 'initrd'<br/>(initrd.tar, ustar)"] --> MB["mb1_find_module_by_string('initrd')<br/>boot/multiboot_modules.c"]
    MB --> IV["initrd_vfs_init(start,end)<br/>fs/initrd_vfs.c"]
    IV --> TAR["tar scan: 512-byte headers,<br/>octal sizes (fs/initrd_tar.c helpers)"]
    IV --> ROOT["initrd_vfs_root() : vnode_t*"]
    ROOT --> VI["vfs_init(root)<br/>fs/vfs.c"]
    VI --> API["vfs_open / read / close / stat / ls<br/>32-entry fd table"]
    API --> CONSUMERS["kernel shell: ls cd cat hexdump<br/>user_load: ELF loading"]
```

- The VFS is a vnode interface: `read(vn, off, buf, len)`, `readdir(vn, index, name, max)`, `lookup(dir, name)`. A vnode carries ops, an `fs_data` pointer, size and an `is_dir` flag.
- `vfs_resolve` walks an absolute path component by component through `lookup`. Relative path handling (`.`, `..`, cwd) is done by the kernel shell (`shell_canon_path`), not the VFS. The user shell does no path resolution beyond prefixing `/bin/`.
- There is a single global fd table of 32 entries, used only inside the kernel. User processes have no file descriptors beyond the fixed console semantics of fd 0/1/2; there is no `open` syscall.
- The initrd backend scans the tar on every lookup and directory read (no index). It is read-only.
- The tar is built by `iso.sh` from `initrd/` (including the generated `initrd/bin`), with macOS metadata stripped (`COPYFILE_DISABLE`, `--no-xattrs`, `--exclude=._*`).

## 8. Syscall ABI

Defined in `kernel/include/kernel/syscall.h`, shared by kernel and user programs. Numbers follow
Linux i386.

| Register | Role |
|---|---|
| `eax` | syscall number on entry, return value on exit (`-1` on error) |
| `ebx`, `ecx`, `edx` | arguments 1, 2, 3 |
| `int $0x80` | trap instruction (DPL 3 gate) |

| Nr | Name | Args | Behavior |
|---|---|---|---|
| 1 | `SYS_exit` | `ebx` = status | ends the calling process (zombie until reaped); never returns |
| 2 | `SYS_fork` | none | duplicates the process (deep page copy); returns the child's pid to the parent and 0 to the child, or -1 |
| 3 | `SYS_read` | `ebx` = fd (must be 0), `ecx` = buf, `edx` = len | blocking, line-buffered read from the console; sleeps until a full line is available and returns up to one line (including the `\n`), at most `len` bytes; -1 for another fd or a bad buffer |
| 4 | `SYS_write` | `ebx` = fd (1 or 2), `ecx` = buf, `edx` = len | writes to the VGA terminal via `putchar`; fd 1 and 2 are identical; returns `len`, or -1 for another fd or an invalid buffer |
| 7 | `SYS_waitpid` | `ebx` = pid (or -1), `ecx` = `int*` status (may be 0), `edx` ignored | blocks until a matching child is a zombie; returns its pid and reaps it; -1 if there is no such child |
| 11 | `SYS_execve` | `ebx` = path, `ecx` = `argv` (NULL-terminated, may be 0) | replaces the process image; returns only on failure (-1). Up to 16 args of 128 bytes; `envp` is not passed |
| 20 | `SYS_getpid` | none | the caller's pid |
| other | | | returns -1 |

The header also provides `__syscall3(num, a, b, c)` as an inline helper; the libc wrappers use
it. All user pointers are checked with `user_range_ok` (inside the user region, every page
mapped) before the kernel touches them.

libc wrappers (`libc/unistd/`, `libc/stdlib/exit.c`): `read`, `write`, `getpid`, `fork`,
`execv`, `execve`, `_exit`, `waitpid`, `wait`, `exit`; `sys/wait.h` defines `WEXITSTATUS`
(and a `WIFEXITED` that is always true). `exit` has no atexit handlers; stdio is unbuffered
(`putchar` is a one-byte `write`).

## 9. Toolchain and build pipeline

```mermaid
flowchart TD
    subgraph SCRIPTS["scripts"]
        QS[qemu.sh] --> ISO[iso.sh]
        ISO --> BUILD[build.sh]
        BUILD --> HDR[headers.sh]
        HDR --> CFG[config.sh]
    end
    CFG -. "HOST=i686-elf, CC with --sysroot=sysroot" .-> HDR
    HDR --> H1["make install-headers: libc, kernel<br/>-> sysroot/usr/include"]
    BUILD --> M1["make install: libc (libc.a, libk.a), kernel<br/>-> sysroot/usr/lib, sysroot/boot/myos.kernel"]
    ISO --> TB["toolchain/build.sh<br/>crt0.S -> sysroot/usr/lib/crt0.o"]
    ISO --> W["for each user/*.c:<br/>toolchain/i686-bobliu-gcc -O2<br/>-nostdlib -T user.ld crt0.o prog.c -lc -lgcc<br/>-> initrd/bin/prog (ELF at 0x10000000)"]
    ISO --> TAR["tar initrd/ -> isodir/boot/initrd.tar"]
    W --> TAR
    M1 --> ISOD["isodir/boot/myos.kernel"]
    TAR --> ISOD
    ISO --> GCFG["isodir/boot/grub/grub.cfg<br/>(user shell / kernel shell / noshell)"]
    ISOD --> MK["i686-elf-grub-mkrescue -> bobliu.iso"]
    GCFG --> MK
    MK --> QEMU["qemu-system-i386 -cdrom bobliu.iso"]
    QS --> QEMU
```

- `config.sh` defaults `HOST` to `i686-elf` (`default-host.sh`), `MAKE` to `gmake`, and wraps the compiler with `--sysroot` and `-isystem`. Compilers come from `PATH`; put `~/opt/cross/bin` on it.
- The kernel is linked with `kernel/arch/i386/linker.ld`: load address 1 MiB, page-aligned sections, `__kernel_start`/`__kernel_end` for the PMM, `.init_array`/`.ctors` markers.
- Kernel C is compiled with `-fno-stack-protector`; `libk` is built with `-D__is_libk`. `libc/Makefile` builds `libk.a` (kernel: the freestanding stdio/string/abort objects) and `libc.a` (those plus the `unistd` syscall wrappers and `exit`).
- `toolchain/i686-bobliu-gcc` is not a separate GCC target. It is a wrapper around `i686-elf-gcc` (or `$BOBLIU_CC`) with `-m32 -ffreestanding -fno-pic -fno-stack-protector`, the sysroot headers, and on link: `-nostdlib`, `crt0.o`, `user.ld`, `-lgcc`, and `-lc` only if `sysroot/usr/lib/libc.a` exists.
- `user.ld` places `.text/.rodata/.data/.bss` starting at `0x10000000` (`USER_LO`), entry `_start`.
- GRUB entries written by `iso.sh`: `bobliu (user shell)` (`init=/bin/sh`, first and so default), `bobliu (kernel shell)` (no arguments) and `bobliu (noshell)`.
- `test.sh` is a stale scratch script referencing files that no longer exist; it is not part of the build.

## 10. Shells and console

Two shells exist; see the README for the command lists.

**Kernel shell** (`shell/shell.c`). The keyboard IRQ only edits the line buffer. On Enter it
calls `shell_submit(line)`:

- If `proc_kernel_busy()` (pid 0 is blocked waiting on a user process), the line is pushed into
  the console buffer with `console_push_line`, which wakes any process sleeping in `read`.
- Otherwise the line is stored in a one-slot pending buffer (a line arriving while the previous
  one is still pending is dropped).

The pid 0 main loop calls `shell_poll()`, which runs `shell_on_line`: it tokenizes on spaces
(max 16 arguments, 128-character line), dispatches through a static command table and prints the
prompt `bobliu:<cwd>> `. Because it runs with interrupts enabled outside any IRQ, commands may
block. `exec <path> [args...]` calls `proc_spawn` and `proc_wait`; `ps` calls `proc_dump`.

**User shell** (`user/sh.c`). A normal ring 3 program. It prints `$ `, `read`s a line from fd 0,
splits it on spaces (max 16 arguments), handles the built-in `exit`, and otherwise `fork`s; the
child `execv`s `/bin/<cmd>` (or the given path if it contains `/`) and prints
`sh: <cmd>: not found` then exits 127; the parent `waitpid`s and prints `[exit N]` for a nonzero
status. When it exits, `kernel_main`'s `proc_wait` returns, `[init exited]` is printed, and the
kernel shell starts.

**Console input** (`dev/console.c`). A single global 1024-byte ring buffer. `console_push_line`
(from the keyboard IRQ) appends the line plus `\n` (the whole line is dropped if it does not
fit) and wakes sleepers on `WAIT_CONSOLE`. `console_read` sleeps while the buffer is empty, then
copies up to the end of one line or `max` bytes. Echo and backspace editing are done by the
keyboard driver itself, before the line is delivered.

Boot option `noshell` skips `shell_init` and calls `keyboard_enable_shell(false)`, so Enter
does nothing, lines are never submitted and `init=` is ignored.

## 11. Known limitations

- **No copy-on-write.** `fork` allocates a frame and copies 4 KiB for every present user page of the parent (code, data and the 8-page stack), so it is O(size of the process) and can fail with -1 when memory is short.
- **Cooperative blocking only in `waitpid` and `read`.** Preemption happens only for ring 3 code, on the timer (every 5 ticks = 50 ms). Kernel code, including whole syscalls, is never preempted; the kernel context (pid 0) and syscalls run to completion or until they sleep. There is no `sleep`, `yield`, `kill` or signal syscall, and no priorities.
- **No kernel-stack guard page.** Each process has a 16 KiB stack from `kmalloc` in the identity-mapped heap. Overflow silently corrupts the neighbouring heap block. The pid 0 context uses the 16 KiB boot stack, and the TSS bootstrap stack is a static 4 KiB buffer. (GitHub issue #9.)
- **No file syscalls.** There is no `open`, `close`, `lseek` or `brk`/`mmap`; `read` works only on fd 0, `write` only on fd 1 and 2 (identical). There is no per-process fd table. (GitHub issue #8.)
- **Process limits.** At most 16 processes (including pid 0); pids are not reused within a boot and a process that finds no free slot or no memory gets -1 from `fork`.
- **Argument passing.** `execve` copies at most 16 arguments of 128 bytes each (the path is also limited to 128 bytes); longer ones make it fail with -1. `envp` is ignored and no environment is passed.
- **stdin is one global console buffer.** All processes share it; whichever calls `read` first gets a line. Input is only delivered to user programs while pid 0 is blocked in `waitpid`; otherwise lines go to the kernel shell. A line over the buffer's free space is dropped. There is no raw mode, no EOF (Ctrl-D), no Ctrl-C.
- **`waitpid` is minimal.** The `options` argument is ignored, `WIFEXITED` is always true, and only the low 8 bits of the exit code are kept. Orphans are adopted by pid 0 and reaped only by its main loop. A killed process reports `128 + vector` (139 for page faults).
- **Identity paging for the kernel.** The kernel lives in the low 256 MiB with no higher-half mapping. Page tables and user frames are accessed through that identity map, but the PMM does not cap allocations below 256 MiB, so on a machine with more RAM and exhausted low memory a frame above 256 MiB would be unreachable.
- **Permissions are coarse.** All loaded segments are mapped read/write/user; no NX, no `PF_W` handling. The ELF loader reads the entire file (at most 512 KiB) into a temporary heap buffer.
- **Heap.** 4 MiB total at a fixed address, first-fit (linear scan, no size classes); `kmalloc_aligned` ignores its alignment argument. The `alloc` shell command leaks its block by design.
- **Kernel exceptions halt.** A CPU exception or page fault in ring 0 prints a dump and stops the machine. (In ring 3 only the faulting process is killed.)
- **Interrupt setup is redundant and early.** `gdt_init` and `idt_init` run twice (once in `interrupts_init`, again in `kernel_main` around the TSS), and interrupts are enabled before PMM and paging exist.
- **Input and output.** US-layout scancode set 1 only; extended (`E0`) keys are ignored. Output is VGA text mode only; no serial console.
- **Stack protector.** Disabled for the kernel build; `ssp.c` and the guard exist but `ssp_init` is not called in `kernel_main`.
- **Dead and unused code.** `kernel/arch/i386/elf/elf.c` (`elf_load_from_vfs`, an older loader that still uses the old stack top) is still compiled and linked but nothing calls it; the real loader is `cpu/user_exec.c`. `kernel_old.c`, `boot_old.s`, `linker_old.ld`, `kernel_old.c++`, `kernel/arch/i386/test/*` and the unused `user_code` function in `kernel.c` are not part of any live path.

### Inconsistencies noticed while documenting

- The kernel shell's `help` text omits `uptime`, even though it is registered.
- `grub.cfg` at the repo root (`myos.bin`) is stale; `iso.sh` generates the real one into `isodir/`.
- `elf/elf.c` still redefines `USER_STACK_TOP` (as `0x02000000`) and `USER_STACK_PAGES` (4), conflicting with the `paging.h` values (`0x40000000`, 8) that the live loader uses.
- `test.sh` and the stray top-level `hello`, `initrd_root/` and `initrd.tar` files are leftovers and not used by `iso.sh`.
- `PF_TMP_PT_VA` (`0x00FF0000`) in `interrupts/page_fault.c` is only used by the kernel-fault page-table dump.
