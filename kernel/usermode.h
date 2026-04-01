#pragma once
#include <stdint.h>

/*
 * enter_usermode — switch from Ring 0 to Ring 3.
 *
 *   user_eip   : virtual address of the user code entry point
 *   user_esp   : top of the user-mode stack (must be Ring-3 accessible)
 *
 * After this call the CPU is in CPL=3.  The only way back to Ring 0 is
 * through an interrupt or a syscall (int 0x80).
 *
 * Prerequisites:
 *   - GDT has valid Ring-3 code (0x23) and data (0x2B) descriptors.
 *   - TSS.esp0 / TSS.ss0 point to the kernel stack for this process.
 *   - The user virtual address range is mapped PAGE_USER | PAGE_PRESENT.
 */
void enter_usermode(uint32_t user_eip, uint32_t user_esp);
