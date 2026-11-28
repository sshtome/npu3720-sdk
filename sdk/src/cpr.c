/*
 * cpr.c — Intel NPU 3720 Power, Clocks & Reset control (CPR / AON / NoC).
 */
#include "npu3720_sdk.h"

/* Delay in microseconds using 38.4 MHz TIM_PERF_FREE_CNT (38.4 ticks/us) */
static void cpr_delay_us(u32 us)
{
    u32 ticks = us * 39u;
    u32 start = npu_ticks();
    while ((npu_ticks() - start) < ticks)
        __asm__ volatile("nop");
}

/*
 * Island power-up helper following stock HglCprIslandEnable (@0x85018b74):
 * 1. Clear RST & CLK on master domain
 * 2. Set TRICKLE_EN
 * 3. Ramp delay (10 us for island 5, 7 us for 32/33)
 * 4. Set PWR_EN
 * 5. Poll PWR_STAT != 0 (up to 6 times with 1 us delay)
 * 6. Set CLK
 * 7. Clear ISO_EN
 * 8. Set RST (release reset)
 */
static int cpr_island_enable(u32 island, u32 dom, u32 master_mask, u32 ramp_us)
{
    u32 trickle_reg, pwr_reg, stat_reg, iso_reg;
    u32 mask;
    u32 tries;

    if (island < 32u) {
        mask = (1u << island);
        trickle_reg = NPU_AON_PWR_TRICKLE_EN0;
        pwr_reg     = NPU_AON_PWR_EN0;
        stat_reg    = NPU_AON_PWR_STAT0;
        iso_reg     = NPU_AON_PWR_ISO_EN0;
    } else {
        if (island == 32u)
            mask = 0x00000301u; /* islands 32, 40, 41 */
        else if (island == 33u)
            mask = 0x00000C02u; /* islands 33, 42, 43 */
        else
            mask = (1u << (island - 32u));

        trickle_reg = NPU_AON_PWR_TRICKLE_EN1;
        pwr_reg     = NPU_AON_PWR_EN1;
        stat_reg    = NPU_AON_PWR_STAT1;
        iso_reg     = NPU_AON_PWR_ISO_EN1;
    }

    /* 1. Clear RST & CLK on master domain if provided */
    if (master_mask) {
        if (dom == 0) {
            mmio_write32(NPU_CPR_RST_CLR, master_mask);
            mmio_write32(NPU_CPR_CLK_CLR, master_mask);
        } else if (dom == 2 || dom == 4) {
            mmio_write32(NPU_NCE_CPR_RST_CLR, master_mask);
            mmio_write32(NPU_NCE_CPR_CLK_CLR, master_mask);
        }
    }

    /* 2. Set TRICKLE_EN */
    mmio_write32(trickle_reg, mmio_read32(trickle_reg) | mask);

    /* 3. Ramp delay */
    cpr_delay_us(ramp_us ? ramp_us : 10u);

    /* 4. Set PWR_EN */
    mmio_write32(pwr_reg, mmio_read32(pwr_reg) | mask);

    /* 5. Poll PWR_STAT */
    for (tries = 0; tries < 20u; tries++) {
        if ((mmio_read32(stat_reg) & mask) != 0u)
            break;
        cpr_delay_us(1u);
    }
    if ((mmio_read32(stat_reg) & mask) == 0u)
        return -1;

    /* 6. Set CLK */
    if (master_mask) {
        if (dom == 0)
            mmio_write32(NPU_CPR_CLK_SET, master_mask);
        else if (dom == 2 || dom == 4)
            mmio_write32(NPU_NCE_CPR_CLK_SET, master_mask);
    }

    /* 7. Clear ISO_EN (un-isolate) */
    mmio_write32(iso_reg, mmio_read32(iso_reg) & ~mask);

    /* 8. Set RST (release reset) */
    if (master_mask) {
        if (dom == 0)
            mmio_write32(NPU_CPR_RST_SET, master_mask);
        else if (dom == 2 || dom == 4)
            mmio_write32(NPU_NCE_CPR_RST_SET, master_mask);
    }

    return 0;
}

int npu_cpr_init_nce_spine(void)
{
    u32 tries;

    /* Unmask Island 5 in AON mask */
    mmio_write32(NPU_AON_EN_MASK0, mmio_read32(NPU_AON_EN_MASK0) | 0x20u);

    /* Power up Island 5 (NCE Spine / LeonNN) with master domain 0 mask 0x200 (NCE_MAS) */
    if (cpr_island_enable(5u, 0u, NPU_CPR_CLK_NCE_MAS, 10u) != 0)
        return -1;

    /* Ensure NCE_MAS clock and reset de-asserted */
    mmio_write32(NPU_CPR_CLK_SET, NPU_CPR_CLK_NCE_MAS);
    mmio_write32(NPU_CPR_RST_SET, NPU_CPR_CLK_NCE_MAS);

    /* Configure TOP_NOC Q-channel: enable NCE_AXI (BIT 6 = 0x40) */
    mmio_write32(NPU_TOP_NOC_QREQN, mmio_read32(NPU_TOP_NOC_QREQN) | NPU_TOP_NOC_NCE_AXI);
    for (tries = 0; tries < 100u; tries++) {
        u32 acc = mmio_read32(NPU_TOP_NOC_QACCEPTN);
        u32 den = mmio_read32(NPU_TOP_NOC_QDENY);
        if ((acc & NPU_TOP_NOC_NCE_AXI) && !(den & NPU_TOP_NOC_NCE_AXI))
            break;
        cpr_delay_us(1u);
    }

    /* Configure NCE_NOC_Q_CTRL (0x2F008034): request channels (0x924), wait accept (0x492) */
    mmio_write32(NPU_NCE_CPR_NOC_Q_CTRL, mmio_read32(NPU_NCE_CPR_NOC_Q_CTRL) | 0x924u);
    for (tries = 0; tries < 100u; tries++) {
        u32 q = mmio_read32(NPU_NCE_CPR_NOC_Q_CTRL);
        if ((q & 0x492u) == 0x492u && (q & 0x249u) == 0u)
            break;
        cpr_delay_us(1u);
    }

    /* Enable NCE_SPINE device clocks & resets (domain 3: 0x0C, then 0xF11) */
    mmio_write32(NPU_NCE_CPR_CLK_SET, 0x0Cu);
    mmio_write32(NPU_NCE_CPR_RST_SET, 0x0Cu);
    mmio_write32(NPU_NCE_CPR_CLK_SET, 0xF11u);
    mmio_write32(NPU_NCE_CPR_RST_SET, 0xF11u);

    return 0;
}

int npu_cpr_power_tile0(void)
{
    /* Island 32 (Tile 0, slices 0-1): mask in AON1 is 0x301 (islands 32, 40, 41) */
    if (cpr_island_enable(32u, 2u, 0x00301000u, 7u) != 0)
        return -1;

    /* Set TILE_AUX_SET (0x2F008024): bit 12 shifted by 12 = 0x01 */
    mmio_write32(NPU_NCE_CPR_TILE_AUX_SET, 0x01u);

    /* Enable Tile 0 devices: domain 4 bit 12 (0x1000), domain 2 bit 5 (0x20) */
    mmio_write32(NPU_NCE_CPR_CLK_SET, (1u << 12) | (1u << 5));
    mmio_write32(NPU_NCE_CPR_RST_SET, (1u << 12) | (1u << 5));

    return 0;
}

int npu_cpr_power_tile1(void)
{
    /* Island 33 (Tile 1, slices 2-3): mask in AON1 is 0xC02 (islands 33, 42, 43) */
    if (cpr_island_enable(33u, 2u, 0x00C02000u, 7u) != 0)
        return -1;

    /* Set TILE_AUX_SET (0x2F008024): bit 13 shifted by 12 = 0x02 */
    mmio_write32(NPU_NCE_CPR_TILE_AUX_SET, 0x02u);

    /* Enable Tile 1 devices: domain 4 bit 13 (0x2000), domain 2 bits 6,7 (0xC0) */
    mmio_write32(NPU_NCE_CPR_CLK_SET, (1u << 13) | (3u << 6));
    mmio_write32(NPU_NCE_CPR_RST_SET, (1u << 13) | (3u << 6));

    return 0;
}

int npu_cpr_power_compute_all(void)
{
    if (npu_cpr_init_nce_spine() != 0)
        return -1;
    if (npu_cpr_power_tile0() != 0)
        return -2;
    if (npu_cpr_power_tile1() != 0)
        return -3;
    return 0;
}

u32 npu_cpr_punit_status(void)
{
    return mmio_read32(NPU_PUNIT_WP_CFG_MIRROR);
}
