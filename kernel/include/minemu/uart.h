#ifndef MINEMU_UART_H
#define MINEMU_UART_H

#include <stddef.h>
#include <stdint.h>

/*
 * UART0 console driver.
 *
 * Output is polled: each byte waits for TX ready, then is written to tx_data.
 * Input is interrupt driven: the RX interrupt handler drains every available
 * byte from rx_data into a ring buffer, and the main program pulls bytes out
 * of that buffer with uart_getc().
 */

void uart_init(void);                 /* enable RX interrupts for UART0 */
void uart_putc(char c);               /* blocking write of one byte */
void uart_write(const char *s, size_t n);
void uart_puts(const char *s);        /* write a NUL-terminated string */
int uart_getc(void);                  /* next received byte, or -1 if none */
void uart_irq_handler(void);          /* called by the IRQ dispatcher */

#endif
