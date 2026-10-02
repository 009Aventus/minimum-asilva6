#ifndef MINEMU_KPRINTF_H
#define MINEMU_KPRINTF_H

/* Minimal printf for console logging: supports %s %c %d %u %x %p %%. */
void kprintf(const char *fmt, ...);

#endif
