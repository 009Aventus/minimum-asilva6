/*
 * System call dispatcher and the ioctl syscall.
 *
 * Path: user code runs `svc #0` -> minemu_svc_trampoline (svc_trampoline.S)
 * saves the user's registers in a trap frame -> minemu_svc_dispatch (here)
 * checks the request, does the work, and writes the result into frame->r[0]
 * -> the trampoline restores the registers and returns after the svc.
 */
#include <stdbool.h>
#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/trap.h"
#include "minemu/uart.h"
#include "minemu/user.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000) /* encoding of `svc #0` */

/* The device-address fd / small-op encoding from the HW2 instructions table.
 * The tests use the user_abi.h encoding instead; we accept both. */
#define SPEC_OP_0 UINT32_C(0)
#define SPEC_OP_1 UINT32_C(1)

#define ERR MINEMU_IOCTL_ERROR

/* ---------------------------------------------------------------------------
 * User-pointer validation
 *
 * A user program can pass ANY number as a "pointer". Before the kernel reads
 * or writes through it, every page the range touches must be mapped in the
 * user's page table, marked USER, and allow the access (read or write).
 * ------------------------------------------------------------------------- */
static bool user_range_ok(uint32_t addr, uint32_t len, bool need_write) {
    if (len == 0) {
        return true;
    }
    uint32_t last = addr + len - 1u;
    if (last < addr) {
        return false; /* range wraps around the end of memory */
    }
    uint32_t need = need_write ? MINEMU_PTE_WRITABLE : MINEMU_PTE_READABLE;
    for (uint32_t page = addr & MINEMU_PTE_PAGE_MASK;; page += MINEMU_PAGE_SIZE) {
        uint32_t pte = user_pte_lookup(page);
        if ((pte & MINEMU_PTE_VALID) == 0 || (pte & MINEMU_PTE_USER) == 0 ||
            (pte & need) == 0) {
            return false;
        }
        if (page == (last & MINEMU_PTE_PAGE_MASK)) {
            return true;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Device operations (run in SVC mode, so MMIO access is allowed)
 * ------------------------------------------------------------------------- */
static int32_t uart_write_bytes(volatile struct minemu_uart_regs *uart,
                                const char *src, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        while ((uart->status & MINEMU_UART_STATUS_TX_READY) == 0) {
        }
        uart->tx_data = (uint32_t)(uint8_t)src[i];
    }
    return (int32_t)n;
}

static int32_t uart_read_bytes(volatile struct minemu_uart_regs *uart, char *dst,
                               uint32_t n) {
    uint32_t count = 0;
    while (count < n) {
        int c;
        if (uart == MINEMU_UART0) {
            c = uart_getc(); /* UART0 input arrives via the HW1 IRQ ring buffer */
        } else if (uart->status & MINEMU_UART_STATUS_RX_READY) {
            c = (int)(uart->rx_data & 0xffu); /* UART1 is polled directly */
        } else {
            c = -1;
        }
        if (c < 0) {
            break; /* no more data available right now */
        }
        dst[count++] = (char)c;
    }
    return (int32_t)count;
}

/* Which UART does this fd name? NULL if it is not a UART fd. */
static volatile struct minemu_uart_regs *uart_for_fd(uint32_t fd) {
    switch (fd) {
    case MINEMU_FD_UART_IN:
    case MINEMU_FD_UART_OUT:
    case MINEMU_UART0_BASE:
        return MINEMU_UART0;
    case MINEMU_FD_UART_DIAG:
    case MINEMU_UART1_BASE:
        return MINEMU_UART1;
    default:
        return 0;
    }
}

static int32_t ioctl_uart(uint32_t fd, uint32_t op, uint32_t a0, uint32_t a1) {
    volatile struct minemu_uart_regs *uart = uart_for_fd(fd);
    bool is_read = op == MINEMU_IOCTL_UART_READ || (fd >= MINEMU_UART0_BASE && op == SPEC_OP_0);
    bool is_write = op == MINEMU_IOCTL_UART_WRITE || (fd >= MINEMU_UART0_BASE && op == SPEC_OP_1);

    if (op == MINEMU_IOCTL_UART_STATUS) {
        uint32_t status = uart->status;
        return (int32_t)(status & (MINEMU_IOCTL_UART_RX_READY | MINEMU_IOCTL_UART_TX_READY));
    }
    if (!is_read && !is_write) {
        return ERR;
    }
    int32_t n = (int32_t)a1;
    if (n < 0) {
        return ERR;
    }
    if (n > MINEMU_IOCTL_MAX_TRANSFER) {
        n = MINEMU_IOCTL_MAX_TRANSFER; /* bound the work done per syscall */
    }
    /* Validate BEFORE touching the device, so a bad pointer has no effect. */
    if (!user_range_ok(a0, (uint32_t)n, is_read)) {
        return ERR;
    }
    if (is_read) {
        return uart_read_bytes(uart, (char *)(uintptr_t)a0, (uint32_t)n);
    }
    return uart_write_bytes(uart, (const char *)(uintptr_t)a0, (uint32_t)n);
}

static int32_t ioctl_rng(uint32_t op, uint32_t a0) {
    bool next = op == MINEMU_IOCTL_RNG_NEXT || op == SPEC_OP_1;
    bool state = op == MINEMU_IOCTL_RNG_STATE;
    bool seed = op == MINEMU_IOCTL_RNG_SEED || op == SPEC_OP_0;

    if (seed) {
        MINEMU_RNG->seed = a0;
        return 0;
    }
    if (!next && !state) {
        return ERR;
    }
    /* Output pointer: must be 4-byte aligned and writable by the user. */
    if ((a0 & 3u) != 0 || !user_range_ok(a0, sizeof(uint32_t), true)) {
        return ERR; /* checked before reading data, which advances the RNG */
    }
    *(uint32_t *)(uintptr_t)a0 = next ? MINEMU_RNG->data : MINEMU_RNG->state;
    return 0;
}

static int32_t sys_ioctl(struct minemu_trap_frame *frame) {
    uint32_t fd = frame->r[0];
    uint32_t op = frame->r[1];
    uint32_t a0 = frame->r[2];
    uint32_t a1 = frame->r[3];

    if (uart_for_fd(fd) != 0) {
        return ioctl_uart(fd, op, a0, a1);
    }
    if (fd == MINEMU_FD_RNG || fd == MINEMU_RNG_BASE) {
        return ioctl_rng(op, a0);
    }
    if (fd == MINEMU_FD_TRACE || fd == MINEMU_TRACE_BASE) {
        if (op != MINEMU_IOCTL_TRACE_EVENT && op != SPEC_OP_0) {
            return ERR;
        }
        minemu_trace_event(a0);
        return 0;
    }
    return ERR; /* unknown fd */
}

/* ---------------------------------------------------------------------------
 * Dispatcher: called by minemu_svc_trampoline with the saved user registers.
 * ------------------------------------------------------------------------- */
struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    uint32_t mode = frame->spsr & CPSR_MODE_MASK;

    /* 1. Was this really an SVC, from user mode, running A32 code? */
    if (frame->exception_id != MINEMU_EXCEPTION_SVC || mode != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0) {
        frame->r[0] = (uint32_t)ERR;
        return frame;
    }

    /* 2. Is the instruction the user executed exactly `svc #0`?
     *    (fault_pc = LR_svc - 4 = address of the svc itself; check that the
     *    address is user-readable before reading it.) */
    if ((frame->fault_pc & 3u) != 0 || !user_range_ok(frame->fault_pc, 4, false) ||
        *(const uint32_t *)(uintptr_t)frame->fault_pc != A32_SVC_ZERO) {
        frame->r[0] = (uint32_t)ERR;
        return frame;
    }

    /* 3. Which service? The syscall number is in r7. */
    switch (frame->r[7]) {
    case MINEMU_SYSCALL_IOCTL:
        frame->r[0] = (uint32_t)sys_ioctl(frame);
        break;
    default:
        frame->r[0] = (uint32_t)ERR; /* unknown syscall */
        break;
    }
    return frame; /* resume the same program */
}

