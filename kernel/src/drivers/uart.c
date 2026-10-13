#include "minemu/uart.h"

#include "minemu/irq.h"
#include "minemu/platform.h"

/* Ring buffer shared between the IRQ handler (producer) and main (consumer). */
#define RX_BUF_SIZE 256u /* power of two so we can wrap with a mask */

static volatile char rx_buf[RX_BUF_SIZE];
static volatile uint32_t rx_head; /* next slot the IRQ handler writes */
static volatile uint32_t rx_tail; /* next slot main reads */

void uart_init(void) {
    rx_head = 0;
    rx_tail = 0;
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_INTERRUPT->enable |= UINT32_C(1) << MINEMU_IRQ_UART0;
}

void uart_putc(char c) {
    while ((MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) == 0) {
        /* wait for room in the transmitter */
    }
    MINEMU_UART0->tx_data = (uint32_t)(uint8_t)c;
}

void uart_write(const char *s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        uart_putc(s[i]);
    }
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

/*
 * Runs in IRQ mode with IRQs masked, so it cannot race with itself.
 * Must drain every pending byte, otherwise the RX interrupt stays asserted
 * and fires again immediately after EOI.
 */
void uart_irq_handler(void) {
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        char c = (char)(MINEMU_UART0->rx_data & 0xffu);
        uint32_t next = (rx_head + 1u) & (RX_BUF_SIZE - 1u);
        if (next != rx_tail) { /* drop the byte if the buffer is full */
            rx_buf[rx_head] = c;
            rx_head = next;
        }
    }
}

/*
 * Runs in the main program, which the IRQ handler can interrupt at any time.
 * Interrupts are disabled while touching the shared indices so the handler
 * never sees a half-updated buffer.
 */
int uart_getc(void) {
    int c = -1;
    /* Save the current IRQ mask instead of blindly re-enabling: syscalls call
     * this from SVC mode, where IRQs are already masked and must stay masked. */
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    minemu_irq_disable();
    if (rx_tail != rx_head) {
        c = (unsigned char)rx_buf[rx_tail];
        rx_tail = (rx_tail + 1u) & (RX_BUF_SIZE - 1u);
    }
    if ((cpsr & UINT32_C(0x80)) == 0) { /* I bit was clear: IRQs were on */
        minemu_irq_enable();
    }
    return c;
}
