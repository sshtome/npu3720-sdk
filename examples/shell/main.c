#include "npu3720_sdk.h"

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int starts_with(const char *s, const char *pfx)
{
    while (*pfx) {
        if (*s != *pfx)
            return 0;
        s++;
        pfx++;
    }
    return 1;
}

static int parse_hex(const char *s, u32 *out)
{
    u32 val = 0;
    int digits = 0;

    while (*s == ' ')
        s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    while (*s) {
        char c = *s++;
        u32 d;
        if (c >= '0' && c <= '9')
            d = (u32)(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = (u32)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            d = (u32)(c - 'A' + 10);
        else if (c == ' ')
            break;
        else
            return -1;
        val = (val << 4) | d;
        digits++;
    }
    if (digits == 0)
        return -1;
    *out = val;
    return 0;
}

static void on_console_line(const char *line)
{
    if (str_eq(line, "ping")) {
        npu_log_puts("pong\n");
    } else if (str_eq(line, "info")) {
        npu_log_puts("NPU3720-SDK v0.1.0 NW=8 freq=");
        npu_log_dec(npu_timer_freq_hz());
        npu_log_puts(" Hz\n");
    } else if (str_eq(line, "ticks")) {
        npu_log_puts("ticks=0x");
        npu_log_hex(npu_ticks());
        npu_log_puts(" uptime=");
        npu_log_dec(npu_uptime_ms());
        npu_log_puts(" ms\n");
    } else if (str_eq(line, "jsm")) {
        npu_log_puts("jsm_count=");
        npu_log_dec(npu_jsm_count());
        npu_log_puts(" last_type=0x");
        npu_log_hex(npu_jsm_last_type());
        npu_log_puts("\n");
    } else if (starts_with(line, "peek ")) {
        u32 addr = 0, val = 0;
        if (parse_hex(line + 5, &addr) != 0) {
            npu_log_puts("usage: peek <hex_addr>\n");
            return;
        }
        npu_log_puts("[0x");
        npu_log_hex(addr);
        npu_log_puts("] = ");
        if (npu_safe_read32(addr, &val) == 0) {
            npu_log_puts("0x");
            npu_log_hex(val);
            npu_log_puts("\n");
        } else {
            npu_log_puts("FAULT (trap 0x09)\n");
        }
    } else if (str_eq(line, "help")) {
        npu_log_puts("commands: ping | info | ticks | jsm | peek <hex> | help\n");
    } else {
        npu_log_puts("unknown cmd: ");
        npu_log_puts(line);
        npu_log_puts("\n");
    }
}

void kmain(void)
{
    npu_init("NPU3720-SHELL");
    npu_console_register(on_console_line);
    npu_log_puts("npu3720-sdk shell ready (type 'help')\n");

    for (;;)
        npu_poll();
}
