#include "minemu/msh.h"

#include <stddef.h>
#include <stdint.h>

#include "minemu/uart.h"

#define MSH_LINE_MAX 20u /* bytes per line, not counting the '\n' */
#define MSH_PROMPT "msh> "

/*
 * Line state. Spaces are normalized as they arrive: leading spaces and
 * repeated spaces are never stored, so the buffer only ever holds
 * "word word word" (possibly with one trailing space).
 */
static char line[MSH_LINE_MAX];
static size_t line_len;
static int line_overflow; /* set once a line exceeds MSH_LINE_MAX */

static int streq(const char *a, size_t a_len, const char *b) {
    size_t i = 0;
    for (; i < a_len; ++i) {
        if (b[i] == '\0' || a[i] != b[i]) {
            return 0;
        }
    }
    return b[i] == '\0';
}

static void msh_execute(void) {
    size_t len = line_len;
    if (len > 0 && line[len - 1] == ' ') {
        --len; /* drop the single trailing space, if any */
    }
    if (len == 0) {
        return; /* empty or all-space line */
    }

    size_t word_len = 0;
    while (word_len < len && line[word_len] != ' ') {
        ++word_len;
    }

    if (streq(line, word_len, "echo")) {
        if (word_len < len) {
            /* skip the separator space; the rest is already single-spaced */
            uart_write(&line[word_len + 1], len - word_len - 1);
        }
        uart_putc('\n');
    } else {
        uart_puts("command not found: ");
        uart_write(line, word_len);
        uart_putc('\n');
    }
}

static void msh_handle_byte(char c) {
    if (c == '\n') {
        if (!line_overflow) {
            msh_execute();
        }
        /* overlong lines are discarded entirely */
        line_len = 0;
        line_overflow = 0;
        uart_puts(MSH_PROMPT);
        return;
    }

    if (line_overflow) {
        return; /* ignore everything until the newline */
    }

    if (c == 0x08 || c == 0x7f) {
        if (line_len > 0) {
            --line_len; /* backspace on an empty line is ignored */
        }
        return;
    }

    if (c == ' ' && (line_len == 0 || line[line_len - 1] == ' ')) {
        return; /* skip leading and repeated spaces */
    }

    if (line_len == MSH_LINE_MAX) {
        line_overflow = 1;
        return;
    }
    line[line_len++] = c;
}

void msh_run(void) {
    line_len = 0;
    line_overflow = 0;
    uart_puts(MSH_PROMPT);
    for (;;) {
        int c = uart_getc();
        if (c < 0) {
            __asm__ volatile("nop"); /* nothing received yet */
            continue;
        }
        msh_handle_byte((char)c);
    }
}
