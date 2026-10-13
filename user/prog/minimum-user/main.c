/*
 * msh, the minimum shell, running as a USER-MODE program (HW2 Task 2).
 *
 * Same behavior as the HW1 kernel shell, but this code cannot touch the UART
 * registers: every byte in or out goes through the ioctl syscall, which traps
 * into the kernel with `svc #0`.
 */
#include <stddef.h>
#include <stdint.h>

#include "minemu/user_abi.h"

/*
 * Line capacity. The spec requires lines of at least 20 bytes; the course's
 * msh-echo test sends a 25-byte line ("echo hello from user mode"), so we
 * allow 64. Anything longer is "overlong" and discarded up to the '\n'.
 */
#define MSH_LINE_MAX 64u
#define MSH_PROMPT "msh> "

/* ---- console I/O through the ioctl syscall ---- */

static void out(const char *s, size_t n) {
    while (n > 0) {
        int32_t written = ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)s,
                                (uint32_t)n);
        if (written <= 0) {
            return; /* kernel refused the write; nothing more we can do */
        }
        s += written;
        n -= (size_t)written;
    }
}

static void puts_str(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    out(s, n);
}

/* Block (by polling the kernel) until one byte of input arrives. */
static char getc_blocking(void) {
    char c;
    while (ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ, (void *)&c, 1u) != 1) {
        /* no input yet: ask again */
    }
    return c;
}

/* ---- shell logic (ported from HW1 kernel/src/core/msh.c) ---- */

/*
 * Spaces are normalized as they arrive: leading and repeated spaces are never
 * stored, so the buffer only holds "word word word" (maybe one trailing space).
 */
static char line[MSH_LINE_MAX];
static size_t line_len;
static int line_overflow; /* set once a line exceeds MSH_LINE_MAX */

static int word_is(const char *word, size_t word_len, const char *name) {
    size_t i = 0;
    for (; i < word_len; ++i) {
        if (name[i] == '\0' || word[i] != name[i]) {
            return 0;
        }
    }
    return name[i] == '\0';
}

static void execute(void) {
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

    if (word_is(line, word_len, "echo")) {
        if (word_len < len) {
            out(&line[word_len + 1], len - word_len - 1); /* already single-spaced */
        }
        out("\n", 1);
    } else {
        puts_str("command not found: ");
        out(line, word_len);
        out("\n", 1);
    }
}

static void handle_byte(char c) {
    if (c == '\n') {
        if (!line_overflow) {
            execute();
        }
        /* overlong lines are discarded entirely */
        line_len = 0;
        line_overflow = 0;
        puts_str(MSH_PROMPT);
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

void minemu_user_main(void) {
    line_len = 0;
    line_overflow = 0;
    puts_str(MSH_PROMPT);
    for (;;) {
        handle_byte(getc_blocking());
    }
}
