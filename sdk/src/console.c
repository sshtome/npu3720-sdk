/*
 * console.c — H2D 0xCCxxxxxx word receiver (2 ASCII bytes/word in bits [21:6]).
 */
#include "npu3720_sdk.h"

#define CONSOLE_BUF_MAX 128u

static char line_buf[CONSOLE_BUF_MAX];
static u32 line_len;
static npu_console_cb_t user_cb;

void npu_console_register(npu_console_cb_t cb)
{
    user_cb = cb;
}

static void push_char(char c)
{
    if (c == '\0')
        return;

    if (c == '\n' || c == '\r') {
        if (line_len > 0) {
            line_buf[line_len] = '\0';
            if (user_cb)
                user_cb(line_buf);
            line_len = 0;
        }
        return;
    }

    if (line_len + 1u < CONSOLE_BUF_MAX)
        line_buf[line_len++] = c;
}

void npu_console_rx_word(u32 word)
{
    char b0 = (char)((word >> 6) & 0xFFu);
    char b1 = (char)((word >> 14) & 0xFFu);

    push_char(b0);
    push_char(b1);
}
