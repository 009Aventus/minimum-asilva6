#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/uart.h"
#include "minemu/user.h"

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }

    /* UART0 input is interrupt driven (HW1 driver); enable its RX IRQ. IRQs
     * are unmasked for the user program by minemu_enter_user (SPSR = USR). */
    uart_init();

    /* Load the user program and drop into user mode. */
    uint32_t entry = 0;
    if (!user_map_system_rom(boot_info) || !user_load(boot_info, "minimum-user", &entry)) {
        minemu_trace_event(UINT32_C(0xbad10ad0));
        minemu_fail_stop();
    }
    minemu_enter_user(entry, USER_STACK_TOP);
}
