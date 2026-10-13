#include <stdint.h>

#include "minemu/user_abi.h"

/* HW2 Task 1: runs in USR mode. It cannot touch the UART directly, so it
 * asks the kernel through the ioctl syscall (svc #0, r7 = SYS_IOCTL). */
void minemu_user_main(void) {
    static const char message[] = "hello world\n";
    (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)message,
                sizeof(message) - 1U);
    for (;;) {
        __asm__ volatile("nop");
    }
}
