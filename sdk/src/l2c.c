/*
 * l2c.c — LeonRT L1 D-cache flush (ASI 0x11) and GRLIB L2C (0x27000000).
 */
#include "npu3720_sdk.h"

static int l1_flush_enabled = 1;
static int l2c_mtrr_active;
static u32 l1_ccr_init;
static u32 l2c_ctrl_init;
static u32 l2c_stat_init;

u32 npu_l1_ccr_read(void)
{
    u32 val;
    __asm__ volatile("lda [%%g0] 0x02, %0" : "=r"(val) :: "memory");
    return val;
}

void npu_l1_ccr_write(u32 val)
{
    __asm__ volatile("nop; nop; sta %0, [%%g0] 0x02" :: "r"(val) : "memory");
}

u32 npu_l1_ccr_initial(void)
{
    return l1_ccr_init;
}

void npu_l1_dcache_flush(void)
{
    __asm__ volatile("sta %%g0, [%%g0] 0x11" ::: "memory");
}

void npu_l1_set_flush_enabled(int en)
{
    l1_flush_enabled = en ? 1 : 0;
}

int npu_l1_is_flush_enabled(void)
{
    return l1_flush_enabled;
}

void npu_l2c_clean_range(u32 addr, u32 size)
{
    u32 stat = mmio_read32(NPU_GRLIB_L2C_STAT);
    u32 line = (stat & (1u << 24)) ? 64u : 32u;
    u32 end, cur;

    if (size == 0u)
        return;

    end = addr + size - 1u;
    cur = addr & ~(line - 1u);
    while (cur <= end) {
        mmio_write32(NPU_GRLIB_L2C_FLUSH, cur | NPU_L2C_OP_CLEAN_LINE);
        if (cur > 0xFFFFFFFFu - line)
            break;
        cur += line;
    }
    (void)mmio_read32(NPU_GRLIB_L2C_CTRL);
}

void npu_l2c_flush_all(void)
{
    npu_l1_dcache_flush();
    mmio_write32(NPU_GRLIB_L2C_FLUSH, NPU_L2C_OP_FLUSH_ALL);
    (void)mmio_read32(NPU_GRLIB_L2C_CTRL);
}

void npu_l2c_set_mtrr(u32 idx, u32 addr, u32 mask, u32 access_ctrl, int wp, int enable)
{
    u32 stat = mmio_read32(NPU_GRLIB_L2C_STAT);
    u32 n_mtrr = (stat >> 16) & 0x3Fu;
    u32 val;

    if (idx >= n_mtrr)
        return;

    val = (addr & 0xFFFC0000u)
        | ((access_ctrl & 0x3u) << 16)
        | ((mask >> 16) & 0xFFFCu)
        | (wp ? 2u : 0u)
        | (enable ? 1u : 0u);
    mmio_write32(NPU_GRLIB_L2C_MTRR(idx), val);
}

int npu_l2c_configure_shared_uncached(void)
{
    u32 n_mtrr, i;

    /* Match stock hard_reset: flush L1 I/D caches and configure CCR before L2C enable. */
    l1_ccr_init = npu_l1_ccr_read();
    __asm__ volatile(".word 0x81D82000; nop; nop; nop" ::: "memory");
    npu_l1_dcache_flush();
    npu_l1_ccr_write(NPU_L1_CCR_DEFAULT);

    l2c_ctrl_init = mmio_read32(NPU_GRLIB_L2C_CTRL);
    l2c_stat_init = mmio_read32(NPU_GRLIB_L2C_STAT);
    n_mtrr = (l2c_stat_init >> 16) & 0x3Fu;
    if (n_mtrr == 0u)
        return -1;

    /* Flush and disable L2C before updating MTRRs so stale lines are not left behind. */
    if (l2c_ctrl_init & NPU_L2C_CTRL_EN) {
        npu_l1_dcache_flush();
        mmio_write32(NPU_GRLIB_L2C_FLUSH, NPU_L2C_OP_FLUSH_ALL);
        (void)mmio_read32(NPU_GRLIB_L2C_CTRL);
        mmio_write32(NPU_GRLIB_L2C_CTRL, l2c_ctrl_init & ~NPU_L2C_CTRL_EN);
    }

    mmio_write32(NPU_GRLIB_L2C_MTRR(0u), NPU_L2C_MTRR_SHARED_UNCACHED);
    for (i = 1u; i < n_mtrr; i++)
        mmio_write32(NPU_GRLIB_L2C_MTRR(i), 0u);

    mmio_write32(NPU_GRLIB_L2C_FLUSH, NPU_L2C_OP_INV_ALL);
    (void)mmio_read32(NPU_GRLIB_L2C_CTRL);
    mmio_write32(NPU_GRLIB_L2C_CTRL, NPU_L2C_CTRL_ENABLE_VAL);
    (void)mmio_read32(NPU_GRLIB_L2C_CTRL);

    l2c_mtrr_active = 1;
    return 0;
}

int npu_l2c_is_mtrr_active(void)
{
    return l2c_mtrr_active;
}

u32 npu_l2c_ctrl_initial(void)
{
    return l2c_ctrl_init;
}

u32 npu_l2c_stat_initial(void)
{
    return l2c_stat_init;
}
