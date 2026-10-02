#include "minemu/kprintf.h"

#include <stdarg.h>
#include <stdint.h>

#include "minemu/uart.h"

static void print_unsigned(uint32_t value, uint32_t base) {
    char digits[12];
    int n = 0;
    do {
        uint32_t d = value % base;
        digits[n++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
        value /= base;
    } while (value != 0);
    while (n > 0) {
        uart_putc(digits[--n]);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (; *fmt != '\0'; ++fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt);
            continue;
        }
        ++fmt;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(args, const char *);
            uart_puts(s != 0 ? s : "(null)");
            break;
        }
        case 'c':
            uart_putc((char)va_arg(args, int));
            break;
        case 'd': {
            int v = va_arg(args, int);
            if (v < 0) {
                uart_putc('-');
                print_unsigned((uint32_t)(-(v + 1)) + 1u, 10);
            } else {
                print_unsigned((uint32_t)v, 10);
            }
            break;
        }
        case 'u':
            print_unsigned(va_arg(args, unsigned int), 10);
            break;
        case 'p':
            uart_puts("0x");
            print_unsigned((uint32_t)(uintptr_t)va_arg(args, void *), 16);
            break;
        case 'x':
            print_unsigned(va_arg(args, unsigned int), 16);
            break;
        case '%':
            uart_putc('%');
            break;
        case '\0':
            --fmt; /* trailing '%': stop cleanly */
            break;
        default:
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }
    }
    va_end(args);
}
