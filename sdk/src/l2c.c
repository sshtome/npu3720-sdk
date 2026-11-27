/*
 * l2c.c — LeonRT L1 D-cache flush (ASI 0x11) and GRLIB L2C (0x27000000).
 */
#include "npu3720_sdk.h"

void npu_l1_dcache_flush(void)
{
    __asm__ volatile("sta %%g0, [%%g0] 0x11" ::: "memory");
}

void npu_l2c_clean_range(u32 addr, u32 size)
{
    u32 stat = mmio_read32(NPU_GRLIB_L2C_STAT);
    u32 line = (stat & (1u << 24)) ? 64u : 32u;
    u32 end, cur;

    if (size == 0)
        return;

    end = addr + size - 1u;
    cur = addr & ~(line - 1u);
    while (cur <= end) {
        mmio_write32(NPU_GRLIB_L2C_FLUSH, cur | NPU_L2C_OP_CLEAN_LINE);
        if (cur > 0xFFFFFFFFu - line)
            break;
        cur += line;
    }
}

void npu_l2c_flush_all(void)
{
    npu_l1_dcache_flush();
    mmio_write32(NPU_GRLIB_L2C_FLUSH, NPU_L2C_OP_FLUSH_ALL);
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
