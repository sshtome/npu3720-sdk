/*
 * cmx.c — Intel NPU 3720 NNCMX SRAM driver & validation (2x2 MB tiles).
 */
#include "npu3720_sdk.h"

int npu_cmx_probe(void)
{
    u32 val0 = 0, val1 = 0;
    if (npu_safe_read32(NPU_CMX_TILE0_BASE, &val0) != 0)
        return -1;
    if (npu_safe_read32(NPU_CMX_TILE1_BASE, &val1) != 0)
        return -2;
    return 0;
}

int npu_cmx_test_rw_integrity(void)
{
    /* Test representative spots across all 4 slices (1 MB each) */
    static const u32 test_offsets[] = {
        0x000000u, /* Slice 0 start */
        0x080000u, /* Slice 0 mid */
        0x100000u, /* Slice 1 start */
        0x180000u, /* Slice 1 mid */
        0x200000u, /* Slice 2 start (Tile 1) */
        0x280000u, /* Slice 2 mid */
        0x300000u, /* Slice 3 start */
        0x380000u, /* Slice 3 mid */
        0x3FFFC0u  /* Slice 3 end */
    };
    const u32 num_offsets = sizeof(test_offsets) / sizeof(test_offsets[0]);
    u32 i;

    /* 1. Write unique pattern to each location */
    for (i = 0; i < num_offsets; i++) {
        volatile u32 *ptr = (volatile u32 *)(NPU_CMX_BASE + test_offsets[i]);
        u32 pattern = 0xA5A50000u ^ test_offsets[i] ^ (i * 0x11111111u);
        *ptr = pattern;
    }

    /* 2. Read back and verify (also verifies absence of aliasing between tiles) */
    for (i = 0; i < num_offsets; i++) {
        volatile u32 *ptr = (volatile u32 *)(NPU_CMX_BASE + test_offsets[i]);
        u32 expected = 0xA5A50000u ^ test_offsets[i] ^ (i * 0x11111111u);
        u32 actual = *ptr;
        if (actual != expected)
            return -(int)(i + 1);
    }

    /* 3. Inverted pattern pass */
    for (i = 0; i < num_offsets; i++) {
        volatile u32 *ptr = (volatile u32 *)(NPU_CMX_BASE + test_offsets[i]);
        u32 pattern = ~(0xA5A50000u ^ test_offsets[i] ^ (i * 0x11111111u));
        *ptr = pattern;
    }
    for (i = 0; i < num_offsets; i++) {
        volatile u32 *ptr = (volatile u32 *)(NPU_CMX_BASE + test_offsets[i]);
        u32 expected = ~(0xA5A50000u ^ test_offsets[i] ^ (i * 0x11111111u));
        u32 actual = *ptr;
        if (actual != expected)
            return -(int)(100 + i + 1);
    }

    return 0;
}

static inline u32 calc_mbs(u32 bytes, u32 dt_ticks)
{
    /*
     * Rate = (bytes / (dt_ticks / 38400000)) / (1024 * 1024)
     *      = (bytes * 38400000) / (dt_ticks * 1048576)
     *      = (bytes * 375) / (dt_ticks * (1024 / 100))
     * For bytes = 65536:
     *      (65536 * 38400000) / (dt_ticks * 1048576) = 2400000 / dt_ticks
     */
    if (dt_ticks == 0)
        dt_ticks = 1;
    return (bytes == 65536u) ? (2400000u / dt_ticks) : ((u32)(((u64)bytes * 38400000ull) / 1048576ull) / dt_ticks);
}

void npu_cmx_benchmark(u32 *cmx_write_mbs, u32 *cmx_read_mbs, u32 *ddr_write_mbs, u32 *ddr_read_mbs)
{
    /* Benchmark 64 KB block transfers */
    const u32 test_bytes = 64u * 1024u;
    const u32 test_words = test_bytes / 4u;
    volatile u32 *cmx_dst = (volatile u32 *)NPU_CMX_BASE;
    /* Use uncached shared DDR area (0x90100000, 64 KB) */
    volatile u32 *ddr_dst = (volatile u32 *)(NPU_SHARED_REGION_BASE + 0x00100000u);
    u32 t0, t1, dt;
    u32 i;
    volatile u32 dummy = 0;

    /* CMX Write */
    t0 = npu_ticks();
    for (i = 0; i < test_words; i++)
        cmx_dst[i] = i;
    t1 = npu_ticks();
    dt = t1 - t0;
    if (cmx_write_mbs)
        *cmx_write_mbs = calc_mbs(test_bytes, dt);

    /* CMX Read */
    t0 = npu_ticks();
    for (i = 0; i < test_words; i++)
        dummy += cmx_dst[i];
    t1 = npu_ticks();
    dt = t1 - t0;
    if (cmx_read_mbs)
        *cmx_read_mbs = calc_mbs(test_bytes, dt);

    /* DDR Write */
    t0 = npu_ticks();
    for (i = 0; i < test_words; i++)
        ddr_dst[i] = i;
    t1 = npu_ticks();
    dt = t1 - t0;
    if (ddr_write_mbs)
        *ddr_write_mbs = calc_mbs(test_bytes, dt);

    /* DDR Read */
    t0 = npu_ticks();
    for (i = 0; i < test_words; i++)
        dummy += ddr_dst[i];
    t1 = npu_ticks();
    dt = t1 - t0;
    if (ddr_read_mbs)
        *ddr_read_mbs = calc_mbs(test_bytes, dt);

    (void)dummy;
}
