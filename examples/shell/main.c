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

__attribute__((noinline))
static u32 vol_deep(u32 n, u32 a, u32 b, u32 c, u32 d)
{
    volatile u32 va = a;
    volatile u32 vb = b;
    volatile u32 vc = c;
    volatile u32 vd = d;
    volatile u32 vn = n;
    u32 res;

    if (vn == 0u)
        return va + vb + vc + vd;

    res = vol_deep(vn - 1u, va + 1u, vb + 2u, vc + 3u, vd + 4u);
    return res + va + vb + vc + vd;
}

static void on_console_line(const char *line)
{
    if (str_eq(line, "ping")) {
        npu_log_puts("pong\n");
    } else if (str_eq(line, "info")) {
        u32 l2c_ctrl = 0, l2c_stat = 0, mtrr0 = 0;
        npu_safe_read32(NPU_GRLIB_L2C_CTRL,     &l2c_ctrl);
        npu_safe_read32(NPU_GRLIB_L2C_STAT,     &l2c_stat);
        npu_safe_read32(NPU_GRLIB_L2C_MTRR(0u), &mtrr0);
        npu_log_puts("NPU3720-SDK v0.1.0 NW=8 freq=");
        npu_log_dec(npu_timer_freq_hz());
        npu_log_puts(" Hz L2C_CTRL=0x");
        npu_log_hex(l2c_ctrl);
        npu_log_puts(" L2C_STAT=0x");
        npu_log_hex(l2c_stat);
        npu_log_puts(" MTRR0=0x");
        npu_log_hex(mtrr0);
        npu_log_puts(" mtrr=");
        npu_log_dec((u32)npu_l2c_is_mtrr_active());
        npu_log_puts(" l1flush=");
        npu_log_dec((u32)npu_l1_is_flush_enabled());
        npu_log_puts(" ccr=0x");
        npu_log_hex(npu_l1_ccr_read());
        npu_log_puts("\n");
    } else if (str_eq(line, "ticks")) {
        u32 lo = 0, hi = 0;
        npu_ticks64(&lo, &hi);
        npu_log_puts("lo=0x");
        npu_log_hex(lo);
        npu_log_puts(" hi=0x");
        npu_log_hex(hi);
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
    } else if (str_eq(line, "l1flush 0")) {
        npu_l1_set_flush_enabled(0);
        npu_log_puts("l1flush=0\n");
    } else if (str_eq(line, "l1flush 1")) {
        npu_l1_set_flush_enabled(1);
        npu_log_puts("l1flush=1\n");
    } else if (starts_with(line, "l1flush")) {
        npu_log_puts("usage: l1flush <0|1>\n");
    } else if (str_eq(line, "help")) {
        npu_log_puts("commands: ping | info | ticks | jsm | peek <hex> | l1flush <0|1> | help\n");
    } else {
        npu_log_puts("unknown cmd: ");
        npu_log_puts(line);
        npu_log_puts("\n");
    }
}

void kmain(void)
{
    u32 lo0 = 0, hi0 = 0, lo1 = 0, hi1 = 0, up_ms;
    u32 i, w;

    npu_init("NPU3720-SHELL");

    npu_ticks64(&lo0, &hi0);
    for (i = 0; i < 1000000u; i++)
        __asm__ volatile("nop");
    npu_ticks64(&lo1, &hi1);
    up_ms = npu_uptime_ms();

    npu_log_puts("boot_ticks: t0=0x");
    npu_log_hex(hi0);
    npu_log_puts(":");
    npu_log_hex(lo0);
    npu_log_puts(" t1=0x");
    npu_log_hex(hi1);
    npu_log_puts(":");
    npu_log_hex(lo1);
    npu_log_puts(" up=");
    npu_log_dec(up_ms);
    npu_log_puts(" ms\n");

    w = vol_deep(12u, 1u, 2u, 3u, 4u);
    if (w == 910u) {
        npu_log_puts("win12=OK\n");
    } else {
        npu_log_puts("win12=FAIL(");
        npu_log_dec(w);
        npu_log_puts(")\n");
    }

    npu_console_register(on_console_line);
    npu_log_puts("npu3720-sdk shell ready (type 'help')\n");

    for (;;)
        npu_poll();
}
