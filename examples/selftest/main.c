#include "npu3720_sdk.h"

static volatile u32 timer0_ticks;

static void on_timer0_irq(u32 irq, void *ctx)
{
    (void)irq;
    (void)ctx;
    npu_timer_tick_ack();
    timer0_ticks++;
}

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

static void on_console_line(const char *line)
{
    if (str_eq(line, "ping"))
        npu_log_puts("pong\n");
    else if (str_eq(line, "stats")) {
        npu_log_puts("irq108=");
        npu_log_dec(npu_irq_count(NPU_IRQ_TIMER0));
        npu_log_puts(" irq12=");
        npu_log_dec(npu_irq_count(NPU_IRQ_H2D_FIFO));
        npu_log_puts(" spur=");
        npu_log_dec(npu_irq_spurious_count());
        npu_log_puts(" up=");
        npu_log_dec(npu_uptime_ms());
        npu_log_puts("ms\n");
    }
}

__attribute__((noinline))
static u32 vol_deep(u32 n, u32 a, u32 b, u32 c, u32 d)
{
    volatile u32 va = a;
    volatile u32 vb = b;
    volatile u32 vc = c;
    volatile u32 vd = d;
    volatile u32 vn = n;
    u32 r = vn ? vol_deep(vn - 1u, va + 1u, vb + 2u, vc + 3u, vd + 4u) : 0u;
    return r + va + vb + vc + vd;
}

__attribute__((noinline))
static u32 reg_deep(u32 n, u32 a, u32 b, u32 c, u32 d)
{
    u32 p = a * 3u + b;
    u32 q = b * 5u + c;
    u32 r = c * 7u + d;
    u32 s = d * 11u + a;
    u32 t = n ? reg_deep(n - 1u, a + 1u, b + 1u, c + 1u, d + 1u) : 0u;
    return t + p + q + r + s;
}

static u32 vol_expect(u32 n)
{
    return 10u * (n + 1u) + 5u * n * (n + 1u);
}

static u32 reg_expect(u32 n)
{
    return 88u * (n + 1u) + 15u * n * (n + 1u);
}

static int test_l2c_mtrr(void)
{
    u32 ctrl = 0, mtrr0 = 0;
    u32 t0, t1;
    int ok;

    npu_safe_read32(NPU_GRLIB_L2C_CTRL, &ctrl);
    npu_safe_read32(NPU_GRLIB_L2C_MTRR(0u), &mtrr0);
    ok = ((ctrl & NPU_L2C_CTRL_ENABLE_VAL) == NPU_L2C_CTRL_ENABLE_VAL) &&
         (npu_l2c_is_mtrr_active() == 1) &&
         (mtrr0 == NPU_L2C_MTRR_SHARED_UNCACHED);

    t0 = npu_ticks();
    for (u32 i = 0; i < 100u; i++)
        npu_log_putc('.');
    npu_log_puts("\n");
    t1 = npu_ticks();

    npu_log_puts("test_l2c_mtrr: ");
    npu_log_puts(ok ? "PASS" : "FAIL");
    npu_log_puts(" ctrl=0x");
    npu_log_hex(ctrl);
    npu_log_puts(" mtrr0=0x");
    npu_log_hex(mtrr0);
    npu_log_puts(" log100_ticks=");
    npu_log_dec(t1 - t0);
    npu_log_puts("\n");
    return ok;
}

static int test_win_deep(void)
{
    static const u32 depths[] = { 2, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20 };
    u32 ndepths = (u32)(sizeof(depths) / sizeof(depths[0]));
    u32 passed = 0;

    for (u32 i = 0; i < ndepths; i++) {
        u32 n = depths[i];
        if (vol_deep(n, 1u, 2u, 3u, 4u) == vol_expect(n))
            passed++;
        if (reg_deep(n, 1u, 2u, 3u, 4u) == reg_expect(n))
            passed++;
    }

    npu_log_puts("test_win_deep: ");
    npu_log_puts((passed == ndepths * 2u) ? "PASS" : "FAIL");
    npu_log_puts(" (");
    npu_log_dec(passed);
    npu_log_puts("/");
    npu_log_dec(ndepths * 2u);
    npu_log_puts(")\n");
    return passed == ndepths * 2u;
}

static int test_fault_skip(void)
{
    /* Probe unmapped AHB holes < 0x80000000 to verify trap 0x09 fault_skip
     * without triggering a host TBU MMU EVTQ recovery reset (>= 0x80000000). */
    static const u32 holes[] = {
        0x40000000u, 0x70000000u, 0x38000000u, 0x25000000u, 0x10000000u
    };
    u32 bad = 0xDEADBEEFu, magic = 0, fault_addr = 0;
    int rc_bad = 0, rc_ok;
    int ok;

    for (u32 i = 0; i < (u32)(sizeof(holes) / sizeof(holes[0])); i++) {
        if (npu_safe_read32(holes[i], &bad) == -1) {
            rc_bad = -1;
            fault_addr = holes[i];
            break;
        }
    }

    rc_ok = npu_safe_read32(NPU_BOOT_PARAMS_ADDR, &magic);
    ok = (rc_bad == -1) && (rc_ok == 0) && (magic == 0x00010000u);

    npu_log_puts("test_fault_skip: ");
    npu_log_puts(ok ? "PASS" : "FAIL");
    npu_log_puts(" addr=0x");
    npu_log_hex(fault_addr);
    npu_log_puts(" magic=0x");
    npu_log_hex(magic);
    npu_log_puts("\n");
    return ok;
}

static int test_timer(void)
{
    u32 up0, up1, lo0 = 0, hi0 = 0, lo1 = 0, hi1 = 0;
    u32 wait_ticks = (npu_timer_freq_hz() / 1000u) * 50u;
    u32 guard = 50000000u;
    int ok;

    up0 = npu_uptime_ms();
    npu_ticks64(&lo0, &hi0);
    while ((npu_ticks() - lo0) < wait_ticks && --guard > 0u)
        ;
    npu_ticks64(&lo1, &hi1);
    up1 = npu_uptime_ms();

    ok = (guard > 0u) && ((lo1 != lo0) || (hi1 != hi0)) && (up1 >= up0 + 45u);
    npu_log_puts("test_timer: ");
    npu_log_puts(ok ? "PASS" : "FAIL");
    npu_log_puts(" up0=");
    npu_log_dec(up0);
    npu_log_puts(" up1=");
    npu_log_dec(up1);
    npu_log_puts(" ms lo0=0x");
    npu_log_hex(lo0);
    npu_log_puts(" lo1=0x");
    npu_log_hex(lo1);
    npu_log_puts("\n");
    return ok;
}

static int test_irq(void)
{
    u32 start_ms = npu_uptime_ms();
    u32 c108 = 0, c12 = 0, spur = 0;
    int ok = 0;

    while ((npu_uptime_ms() - start_ms) < 2000u) {
        c108 = npu_irq_count(NPU_IRQ_TIMER0);
        c12 = npu_irq_count(NPU_IRQ_H2D_FIFO);
        if (c108 >= 10u && c12 >= 1u) {
            ok = 1;
            break;
        }
    }
    spur = npu_irq_spurious_count();

    npu_log_puts("test_irq: ");
    npu_log_puts(ok ? "PASS" : "FAIL");
    npu_log_puts(" irq108=");
    npu_log_dec(c108);
    npu_log_puts(" t0_ticks=");
    npu_log_dec(timer0_ticks);
    npu_log_puts(" irq12=");
    npu_log_dec(c12);
    npu_log_puts(" spur=");
    npu_log_dec(spur);
    npu_log_puts("\n");
    return ok;
}

static int test_cpr_cmx(void)
{
    int cpr_rc, probe_rc, rw_rc;
    u32 c_w = 0, c_r = 0, d_w = 0, d_r = 0;
    u32 punit_wp = npu_cpr_punit_status();

    npu_log_puts("cpr: punit_wp=0x");
    npu_log_hex(punit_wp);
    npu_log_puts("\n");

    cpr_rc = npu_cpr_power_compute_all();
    if (cpr_rc != 0) {
        npu_log_puts("test_cpr_cmx: CPR FAIL rc=");
        npu_log_dec((u32)(-cpr_rc));
        npu_log_puts("\n");
        return 0;
    }

    probe_rc = npu_cmx_probe();
    if (probe_rc != 0) {
        npu_log_puts("test_cpr_cmx: PROBE FAIL rc=");
        npu_log_dec((u32)(-probe_rc));
        npu_log_puts("\n");
        return 0;
    }

    rw_rc = npu_cmx_test_rw_integrity();
    if (rw_rc != 0) {
        npu_log_puts("test_cpr_cmx: RW FAIL rc=");
        npu_log_dec((u32)(-rw_rc));
        npu_log_puts("\n");
        return 0;
    }

    npu_cmx_benchmark(&c_w, &c_r, &d_w, &d_r);

    npu_log_puts("test_cpr_cmx: PASS 4MB CMX r/w OK");
    npu_log_puts(" speed(MB/s): cmx_w=");
    npu_log_dec(c_w);
    npu_log_puts(" cmx_r=");
    npu_log_dec(c_r);
    npu_log_puts(" ddr_w=");
    npu_log_dec(d_w);
    npu_log_puts(" ddr_r=");
    npu_log_dec(d_r);
    npu_log_puts("\n");
    return 1;
}

void kmain(void)
{
    u32 pass_cnt = 0;
    const u32 total = 6;

    npu_init("NPU3720-SELFTEST");
    npu_console_register(on_console_line);

    npu_irq_init();
    npu_irq_register(NPU_IRQ_TIMER0, on_timer0_irq, NULL);
    npu_irq_enable(NPU_IRQ_TIMER0, 7u, 0);
    npu_timer_tick_start(1000u);
    npu_ipc_enable_irq();
    npu_irq_global_enable();

    pass_cnt += (u32)test_l2c_mtrr();
    pass_cnt += (u32)test_win_deep();
    pass_cnt += (u32)test_fault_skip();
    pass_cnt += (u32)test_timer();
    pass_cnt += (u32)test_irq();
    pass_cnt += (u32)test_cpr_cmx();

    if (pass_cnt == total) {
        npu_log_puts("SELFTEST: PASS (6/6)\n");
    } else {
        npu_log_puts("SELFTEST: FAIL (");
        npu_log_dec(pass_cnt);
        npu_log_puts("/6)\n");
    }

    /* Stay alive; H2D FIFO console & JSM messages are serviced by IRQ 12 */
    for (;;)
        __asm__ volatile("nop");
}
