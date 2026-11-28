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


/*
 * DMA test buffers in .bss — placed in DDR by linker.
 * 64 KB each; used for test_dma_ddr (DDR↔CMX round-trip).
 */
static u32 dma_src_buf[64 * 1024 / 4] __attribute__((section(".bss"), aligned(64)));
static u32 dma_dst_buf[64 * 1024 / 4] __attribute__((section(".bss"), aligned(64)));

/* CMX staging area for DDR↔CMX test, well clear of descriptor pool @ 0x2E008000 */
#define DMA_CMX_TMP  0x2E030000u

static u32 calc_crc32(const void *data, u32 len)
{
    const u8 *p = (const u8 *)data;
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < len; i++) {
        crc ^= p[i];
        for (u32 j = 0; j < 8u; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
    }
    return ~crc;
}

static int test_dma_cmx(void)
{
    volatile u32 *src = (volatile u32 *)0x2E010000u;
    volatile u32 *dst = (volatile u32 *)0x2E210000u;
    u32 i, t0, t1, us, mb_s, crc;

    for (i = 0; i < 64u * 1024u / 4u; i++)
        src[i] = 0xA5000000u | i;
    for (i = 0; i < 64u * 1024u / 4u; i++)
        dst[i] = 0u;

    t0 = npu_ticks();
    if (npu_dma_copy_1d(0, 0x2E210000u, 0x2E010000u, 64u * 1024u) != 0) {
        npu_log_puts("test_dma_cmx: FAIL DMA timeout\n");
        return 0;
    }
    t1 = npu_ticks();

    for (i = 0; i < 64u * 1024u / 4u; i++) {
        if (dst[i] != (0xA5000000u | i)) {
            npu_log_puts("test_dma_cmx: FAIL mismatch @ ");
            npu_log_dec(i);
            npu_log_puts(" got=0x");
            npu_log_hex(dst[i]);
            npu_log_puts("\n");
            return 0;
        }
    }

    crc  = calc_crc32((const void *)dst, 64u * 1024u);
    us   = (t1 - t0) / 38u;
    mb_s = (us > 0u) ? (64u * 1000u / us) : 0u;
    npu_log_puts("test_dma_cmx: PASS CMX0->CMX1 64KB ");
    npu_log_dec(us);
    npu_log_puts(" us (");
    npu_log_dec(mb_s);
    npu_log_puts(" MB/s) crc=0x");
    npu_log_hex(crc);
    npu_log_puts("\n");
    return 1;
}

static int test_dma_ddr(void)
{
    u32 i, t0, t1, t2, t3, us_rd, us_wr, mb_s, crc;

    for (i = 0; i < 64u * 1024u / 4u; i++)
        dma_src_buf[i] = 0xBEEF0000u | i;
    for (i = 0; i < 64u * 1024u / 4u; i++)
        dma_dst_buf[i] = 0u;

    t0 = npu_ticks();
    if (npu_dma_copy_1d(0, DMA_CMX_TMP, (u32)(uintptr_t)dma_src_buf, 64u * 1024u) != 0) {
        npu_log_puts("test_dma_ddr: FAIL DDR->CMX timeout\n");
        return 0;
    }
    t1 = npu_ticks();

    t2 = npu_ticks();
    if (npu_dma_copy_1d(0, (u32)(uintptr_t)dma_dst_buf, DMA_CMX_TMP, 64u * 1024u) != 0) {
        npu_log_puts("test_dma_ddr: FAIL CMX->DDR timeout\n");
        return 0;
    }
    t3 = npu_ticks();

    for (i = 0; i < 64u * 1024u / 4u; i++) {
        if (dma_dst_buf[i] != (0xBEEF0000u | i)) {
            npu_log_puts("test_dma_ddr: FAIL mismatch @ ");
            npu_log_dec(i);
            npu_log_puts("\n");
            return 0;
        }
    }

    crc   = calc_crc32((const void *)dma_dst_buf, 64u * 1024u);
    us_rd = (t1 - t0) / 38u;
    us_wr = (t3 - t2) / 38u;
    mb_s  = ((us_rd + us_wr) > 0u) ? (2u * 64u * 1000u / (us_rd + us_wr)) : 0u;
    npu_log_puts("test_dma_ddr: PASS DDR->CMX ");
    npu_log_dec(us_rd);
    npu_log_puts(" us CMX->DDR ");
    npu_log_dec(us_wr);
    npu_log_puts(" us avg ");
    npu_log_dec(mb_s);
    npu_log_puts(" MB/s crc=0x");
    npu_log_hex(crc);
    npu_log_puts("\n");
    return 1;
}

static int test_leonnn(void)
{
    struct npu_lnn_mailbox *mb = npu_leonnn_get_mailbox();
    u32 t0, t1;
    int started;
    u32 magic = 0, asr17 = 0, cnt0 = 0, cnt1 = 0;
    u32 rx_cnt0, rx_cnt1, rx_word = 0;
    int ok_boot = 0, ok_ipc = 0;

    npu_log_puts("test_leonnn: starting LeonNN at 0x");
    npu_log_hex((u32)&npu_lnn_start_entry);
    npu_log_puts("...\n");

    started = npu_leonnn_start((u32)&npu_lnn_start_entry);
    if (started != 0) {
        npu_log_puts("test_leonnn: FAIL start rc=");
        npu_log_dec((u32)(-started));
        npu_log_puts("\n");
        return 0;
    }

    /* Wait for LeonNN mailbox ready (up to 1000 ms) */
    t0 = npu_uptime_ms();
    while ((npu_uptime_ms() - t0) < 1000u) {
        npu_l1_dcache_flush();
        magic = mb->magic;
        asr17 = mb->asr17;
        if (magic == 0x4C4E4E31u && ((asr17 >> 28) & 0xFu) == 2u) {
            ok_boot = 1;
            break;
        }
    }

    if (!ok_boot) {
        u32 dsu_ctrl = 0, dsu_pc = 0, dsu_npc = 0;
        u32 clk_get = 0, rst_get = 0;
        npu_safe_read32(0x2D000000u, &dsu_ctrl);
        npu_safe_read32(0x2D400010u, &dsu_pc);
        npu_safe_read32(0x2D400014u, &dsu_npc);
        npu_safe_read32(NPU_NCE_CPR_CLK_GET, &clk_get);
        npu_safe_read32(NPU_NCE_CPR_RST_GET, &rst_get);
        npu_log_puts("test_leonnn: FAIL boot timeout magic=0x");
        npu_log_hex(magic);
        npu_log_puts(" asr17=0x");
        npu_log_hex(asr17);
        npu_log_puts(" vec=0x");
        npu_log_hex(mmio_read32(NPU_NCE_CPR_LEON_NN_VEC));
        npu_log_puts(" clk=0x");
        npu_log_hex(clk_get);
        npu_log_puts(" rst=0x");
        npu_log_hex(rst_get);
        npu_log_puts(" dsu_ctrl=0x");
        npu_log_hex(dsu_ctrl);
        npu_log_puts(" dsu_pc=0x");
        npu_log_hex(dsu_pc);
        npu_log_puts(" dsu_npc=0x");
        npu_log_hex(dsu_npc);
        npu_log_puts("\n");
        npu_leonnn_stop();
        return 0;
    }

    /* Check counter progress */
    cnt0 = mb->counter;
    t0 = npu_uptime_ms();
    while ((npu_uptime_ms() - t0) < 20u)
        ;
    npu_l1_dcache_flush();
    cnt1 = mb->counter;

    /* Test Bidirectional IPC */
    rx_cnt0 = npu_lnn_get_rx_ipc_count();
    npu_leonnn_send_ipc(0x12345600u);

    t1 = npu_uptime_ms();
    while ((npu_uptime_ms() - t1) < 1000u) {
        rx_cnt1 = npu_lnn_get_rx_ipc_count();
        if (rx_cnt1 > rx_cnt0) {
            rx_word = npu_lnn_get_last_rx_ipc_word();
            if ((rx_word & 0xFF000000u) == NPU_LNN_IPC_TAG) {
                ok_ipc = 1;
                break;
            }
        }
        npu_jsm_poll();
    }

    npu_leonnn_stop();

    npu_log_puts("probe_lnn: srmmu_ctrl=");
    if (mb->fault_flag & 1u) {
        npu_log_puts("FAULT_0x09\n");
    } else {
        npu_log_puts("0x");
        npu_log_hex(mb->srmmu_ctrl);
        npu_log_puts("\n");
    }
    npu_log_puts("probe_lnn: ccr=");
    if (mb->fault_flag & 2u) {
        npu_log_puts("FAULT_0x09\n");
    } else {
        npu_log_puts("0x");
        npu_log_hex(mb->ccr);
        npu_log_puts("\n");
    }

    int ok = ok_boot && (cnt1 > cnt0) && ok_ipc;
    npu_log_puts("test_leonnn: ");
    npu_log_puts(ok ? "PASS" : "FAIL");
    npu_log_puts(" core=");
    npu_log_dec((asr17 >> 28) & 0xFu);
    npu_log_puts(" asr17=0x");
    npu_log_hex(asr17);
    npu_log_puts(" cnt=");
    npu_log_dec(cnt1);
    npu_log_puts(" ipc_rx=0x");
    npu_log_hex(rx_word);
    npu_log_puts("\n");

    return ok;
}

/* ── Safe ASI 0x19 access helpers on LeonRT (protected by trap 0x09) ── */
extern volatile u32 g_fault_skipped;

static int lrt_safe_lda_asi19(u32 reg_offset, u32 *out)
{
    u32 val = 0;
    g_fault_skipped = 0;
    __asm__ volatile("" ::: "memory");
    __asm__ volatile("lda [%1] 0x19, %0" : "=r"(val) : "r"(reg_offset) : "memory");
    __asm__ volatile("" ::: "memory");
    if (g_fault_skipped)
        return -1;
    if (out)
        *out = val;
    return 0;
}

static int lrt_safe_sta_asi19(u32 reg_offset, u32 val)
{
    g_fault_skipped = 0;
    __asm__ volatile("" ::: "memory");
    __asm__ volatile("sta %0, [%1] 0x19" :: "r"(val), "r"(reg_offset) : "memory");
    __asm__ volatile("" ::: "memory");
    if (g_fault_skipped)
        return -1;
    return 0;
}

static void probe_sysinfo_hardware(void)
{
    /* 1. LeonRT SRMMU probe (ASI 0x19: 0x000 CTRL, 0x100 CTP, 0x200 CTX, 0x300 SFSR, 0x400 SFAR) */
    u32 m_ctrl = 0, m_ctp = 0, m_ctx = 0, m_sfsr = 0, m_sfar = 0;
    int f_ctrl = lrt_safe_lda_asi19(0x000u, &m_ctrl);
    int f_ctp  = lrt_safe_lda_asi19(0x100u, &m_ctp);
    int f_ctx  = lrt_safe_lda_asi19(0x200u, &m_ctx);
    int f_sfsr = lrt_safe_lda_asi19(0x300u, &m_sfsr);
    int f_sfar = lrt_safe_lda_asi19(0x400u, &m_sfar);

    (void)f_ctp; (void)f_ctx; (void)f_sfsr; (void)f_sfar;

    if (f_ctrl == 0) {
        npu_log_puts("probe_srmmu_lrt: PRESENT ctrl=0x");
        npu_log_hex(m_ctrl);
        npu_log_puts(" ctp=0x");
        npu_log_hex(m_ctp);
        npu_log_puts(" ctx=0x");
        npu_log_hex(m_ctx);
        npu_log_puts(" sfsr=0x");
        npu_log_hex(m_sfsr);
        npu_log_puts(" sfar=0x");
        npu_log_hex(m_sfar);
        npu_log_puts("\n");

        if ((m_ctrl & 1u) == 0u) {
            /* Test CTP write & readback */
            u32 orig_ctp = m_ctp;
            u32 readback_ctp = 0;
            lrt_safe_sta_asi19(0x100u, 0x12345600u);
            lrt_safe_lda_asi19(0x100u, &readback_ctp);
            lrt_safe_sta_asi19(0x100u, orig_ctp); /* Restore! */
            npu_log_puts("probe_srmmu_lrt: ctp_test wr=0x12345600 rd=0x");
            npu_log_hex(readback_ctp);
            npu_log_puts(" restored=0x");
            npu_log_hex(orig_ctp);
            npu_log_puts("\n");
        }
    } else {
        npu_log_puts("probe_srmmu_lrt: NOT_PRESENT (trap 0x09 on ASI 0x19)\n");
    }

    /* 2. L1 D-cache snooping test: check bit 23 (DS) in CCR (ASI 0x02, 0x00) */
    u32 ccr_orig = npu_l1_ccr_read();
    npu_l1_ccr_write(ccr_orig | (1u << 23));
    u32 ccr_set = npu_l1_ccr_read();
    npu_l1_ccr_write(ccr_orig); /* Restore! */
    int ds_retained = ((ccr_set & (1u << 23)) != 0);
    npu_log_puts("probe_l1_snooping: orig=0x");
    npu_log_hex(ccr_orig);
    npu_log_puts(" set=0x");
    npu_log_hex(ccr_set);
    npu_log_puts(" ds_bit23=");
    npu_log_puts(ds_retained ? "RETAINED(snooping_hw_present)" : "ZERO(snooping_not_synthesized)");
    npu_log_puts("\n");

    /* 3. Core frequency and boot_params */
    const struct npu_boot_params *bp = (const struct npu_boot_params *)NPU_BOOT_PARAMS_ADDR;
    npu_log_puts("probe_freq: bp_freq=");
    npu_log_dec(bp->frequency);
    npu_log_puts(" perf_clk=");
    npu_log_dec(bp->perf_clk_frequency);
    npu_log_puts(" min_pll=");
    npu_log_dec(bp->min_freq_pll_ratio);
    npu_log_puts(" max_pll=");
    npu_log_dec(bp->max_freq_pll_ratio);
    npu_log_puts(" pn_pll=");
    npu_log_dec(bp->pn_freq_pll_ratio);
    npu_log_puts("\n");

    /* Fixed 1,000,000 loop iteration: subcc + bne + nop */
    u32 count = 1000000u;
    u32 t0 = npu_ticks();
    __asm__ volatile(
        "1: subcc %0, 1, %0\n\t"
        "   bne   1b\n\t"
        "   nop\n\t"
        : "+r"(count)
        :
        : "cc", "memory"
    );
    u32 t1 = npu_ticks();
    u32 loop_ticks = t1 - t0;
    u32 time_us = (loop_ticks * 10u) / 384u;
    u32 mhz_2c = (loop_ticks > 0u) ? (76800000u / loop_ticks) : 0u;
    u32 mhz_3c = (loop_ticks > 0u) ? (115200000u / loop_ticks) : 0u;

    npu_log_puts("probe_freq: 1M_loop ticks=");
    npu_log_dec(loop_ticks);
    npu_log_puts(" (");
    npu_log_dec(time_us);
    npu_log_puts(" us) => est_mhz(2c)=");
    npu_log_dec(mhz_2c);
    npu_log_puts(" (3c)=");
    npu_log_dec(mhz_3c);
    npu_log_puts("\n");
}

void kmain(void)
{
    u32 pass_cnt = 0;
    const u32 total = 9;

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
    pass_cnt += (u32)test_leonnn();
    npu_dma_init();
    pass_cnt += (u32)test_dma_cmx();
    pass_cnt += (u32)test_dma_ddr();

    probe_sysinfo_hardware();

    if (pass_cnt == total) {
        npu_log_puts("SELFTEST: PASS (9/9)\n");
    } else {
        npu_log_puts("SELFTEST: FAIL (");
        npu_log_dec(pass_cnt);
        npu_log_puts("/9)\n");
    }

    /* Stay alive; H2D FIFO console & JSM messages are serviced by IRQ 12 */
    for (;;)
        __asm__ volatile("nop");
}
