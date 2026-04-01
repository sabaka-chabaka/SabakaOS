#include "usermode.h"
#include "gdt.h"
#include "tss.h"

/*
 * enter_usermode — jump from Ring 0 to Ring 3 via iret.
 *
 * x86 iret-to-Ring-3 stack frame (top → bottom, i.e. push order):
 *
 *   ss        ← user data selector with RPL=3 (0x2B)
 *   esp       ← user stack pointer
 *   eflags    ← EFLAGS with IF=1 (interrupts enabled in userspace)
 *   cs        ← user code selector with RPL=3 (0x23)
 *   eip       ← user entry point
 *
 * After iret the CPU:
 *   - loads CS from the frame  → CPL becomes 3
 *   - loads SS:ESP from frame  → switches to user stack
 *   - restores EFLAGS          → IF=1 so PIT IRQs keep firing
 *
 * Important: call tss_set_kernel_stack() BEFORE this to ensure the TSS
 * has the correct esp0 for future Ring-0 re-entries.
 */
void enter_usermode(uint32_t user_eip, uint32_t user_esp) {
    /* Flush segment registers to kernel data so the inline asm
       can safely use them before iret overwrites SS. */
    __asm__ volatile(
        /* Load user data selector into all data segment registers. */
        "mov %0,   %%ax  \n"
        "mov %%ax, %%ds  \n"
        "mov %%ax, %%es  \n"
        "mov %%ax, %%fs  \n"
        "mov %%ax, %%gs  \n"

        /* Build the iret frame on the current (kernel) stack. */
        "push %0         \n"   /* ss  = SEL_USER_DATA (0x2B) */
        "push %2         \n"   /* esp = user_esp              */
        "pushf           \n"   /* eflags                      */
        "pop  %%eax      \n"
        "or   $0x200, %%eax \n" /* set IF — interrupts enabled in Ring 3 */
        "push %%eax      \n"   /* eflags (with IF)            */
        "push %1         \n"   /* cs  = SEL_USER_CODE (0x23)  */
        "push %3         \n"   /* eip = user_eip              */
        "iret            \n"
        :
        : "i"(SEL_USER_DATA),
          "i"(SEL_USER_CODE),
          "r"(user_esp),
          "r"(user_eip)
        : "eax"
    );
    /* Never reached — iret transfers control to Ring 3. */
}
