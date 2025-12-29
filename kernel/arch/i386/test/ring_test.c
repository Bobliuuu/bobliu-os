// user_test.c
#include <stdint.h>
#include <arch/i386/pmm.h>
#include <arch/i386/paging.h>

// Optional debug prints (remove if you don't have them)
extern void vga_print(const char* s);
extern void vga_print_hex(uint32_t x);

#ifndef USER_CS
#define USER_CS 0x1B  // selector 0x18 | RPL3  (GDT user code at index 3)
#endif
#ifndef USER_DS
#define USER_DS 0x23  // selector 0x20 | RPL3  (GDT user data at index 4)
#endif

// Syscall test program:
//   eax=1, ebx='A', int 0x80, then spin
static const uint8_t user_prog[] = {
    0xB8, 0x01,0x00,0x00,0x00,   // mov eax,1
    0xBB, 0x41,0x00,0x00,0x00,   // mov ebx,'A'
    0xCD, 0x80,                  // int 0x80
    0xEB, 0xFE                   // jmp $
};

// If your paging flags differ, adjust these defines to match your paging.h
#ifndef P_PRESENT
#define P_PRESENT 0x001u
#endif
#ifndef P_RW
#define P_RW      0x002u
#endif
#ifndef P_USER
#define P_USER    0x004u
#endif

#define USER_BASE      0x00400000u
#define USER_STACK_TOP 0x00800000u   // stack grows down
#define PAGE_SIZE      0x1000u

// A single exported helper to enter ring3.
// Keep this in THIS file only, and call it from your test function.
static void enter_user(uint32_t entry, uint32_t user_stack_top) {
    asm volatile(
        "cli\n"

        // Load user data selectors into segment regs (RPL=3)
        "mov %0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        // Build iret frame: SS, ESP, EFLAGS, CS, EIP
        "pushl %0\n"              // SS = USER_DS
        "pushl %1\n"              // ESP
        "pushfl\n"
        "popl %%eax\n"
        "orl $0x200, %%eax\n"     // IF=1
        "pushl %%eax\n"           // EFLAGS
        "pushl %2\n"              // CS = USER_CS
        "pushl %3\n"              // EIP = entry
        "iret\n"
        :
        : "i"(USER_DS), "r"(user_stack_top), "i"(USER_CS), "r"(entry)
        : "eax", "memory"
    );

    __builtin_unreachable();
}

// Call this from kernel init once paging + GDT/TSS + IDT are ready.
// Requirements BEFORE calling:
//  - GDT has USER code/data descriptors (DPL=3) at selectors matching USER_CS/USER_DS
//  - TSS is loaded with ltr, and esp0/ss0 set to a valid kernel stack
//  - IDT has vector 0x80 with DPL=3 gate (0xEE) and you handle int_no==128
void test_ring3_int80(void) {
    const uint32_t code_va  = USER_BASE;
    const uint32_t stack_va = USER_STACK_TOP - PAGE_SIZE; // one page stack

    // Allocate 2 frames: one for code, one for stack
    uint32_t code_phys  = (uint32_t)pmm_alloc_frame();
    uint32_t stack_phys = (uint32_t)pmm_alloc_frame();

    if (!code_phys || !stack_phys) {
        vga_print("user_test: OOM\n");
        for (;;) asm volatile("hlt");
    }

    // Map as USER pages so ring3 can fetch/stack
    paging_map(code_va,  code_phys,  P_PRESENT | P_RW | P_USER);
    paging_map(stack_va, stack_phys, P_PRESENT | P_RW | P_USER);

    // Copy program bytes into mapped user code page via the *virtual* mapping
    volatile uint8_t* dst = (volatile uint8_t*)code_va;
    for (uint32_t i = 0; i < (uint32_t)sizeof(user_prog); i++) dst[i] = user_prog[i];

    // If you have a TLB invalidate helper, do it here (optional if paging_map already does)
    // paging_invlpg(code_va);
    // paging_invlpg(stack_va);

    vga_print("user_test: enter ring3 eip=");
    vga_print_hex(code_va);
    vga_print("\n");

    // Jump to ring3, stack top at USER_STACK_TOP
    enter_user(code_va, USER_STACK_TOP);

    for (;;) asm volatile("hlt");
}
