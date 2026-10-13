#ifndef MINEMU_USER_H
#define MINEMU_USER_H

#include <stdbool.h>
#include <stdint.h>

#include "minemu/boot.h"

/* Layout of every user address space (virtual addresses). */
#define USER_IMAGE_BASE UINT32_C(0x00400000)   /* lowest address a program may use */
#define USER_STACK_BOTTOM UINT32_C(0x007ff000) /* one 4 KiB stack page ...          */
#define USER_STACK_TOP UINT32_C(0x00800000)    /* ... ending here (sp starts here)  */

/* Map the image in System ROM so the kernel can read the module table. */
bool user_map_system_rom(const struct minemu_boot_info *boot_info);

/* Find the module called `name`, copy it into fresh pages, and build its
 * page table. On success *entry_vaddr is the program's first instruction. */
bool user_load(const struct minemu_boot_info *boot_info, const char *name,
               uint32_t *entry_vaddr);

/* Page-table entry for the user page containing `vaddr`, or 0 if the user
 * address space has no mapping there. Used to validate syscall pointers. */
uint32_t user_pte_lookup(uint32_t vaddr);

/* Drop to USR mode at entry_vaddr with sp = stack_top. Never returns. */
void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top) __attribute__((noreturn));

#endif
