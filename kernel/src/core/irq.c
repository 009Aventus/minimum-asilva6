#include "minemu/irq.h"

#include "minemu/platform.h"
#include "minemu/uart.h"

typedef void (*irq_handler_t)(void);

/* Function table indexed by interrupt source id. */
static irq_handler_t const irq_handlers[] = {
    [MINEMU_IRQ_SYSTICK] = 0,
    [MINEMU_IRQ_UART0] = uart_irq_handler,
    [MINEMU_IRQ_UART1] = 0,
    [MINEMU_IRQ_BLOCK] = 0,
};

#define IRQ_HANDLER_COUNT (sizeof(irq_handlers) / sizeof(irq_handlers[0]))

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;

    if (source == MINEMU_IRQ_NONE) {
        return frame; /* spurious: nothing claimed, nothing to acknowledge */
    }
    if (source < IRQ_HANDLER_COUNT && irq_handlers[source] != 0) {
        irq_handlers[source]();
    }
    MINEMU_INTERRUPT->eoi = source;
    return frame;
}
