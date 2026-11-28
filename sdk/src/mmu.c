/*
 * mmu.c — SPARC Reference MMU (SRMMU) driver for Intel NPU 3720 (LeonRT).
 *
 * Implements 1-level 16 MB section identity mapping and virtual translation.
 */
#include "npu3720_sdk.h"

/*
 * SPARC Reference MMU definitions (RFC 1480 / GRLIB SRMMU).
 *
 * Entry Types (ET, bits [1:0]):
 *   0 = Invalid
 *   1 = Page Table Descriptor (PTD)
 *   2 = Page Table Entry (PTE)
 *
 * Level-1 PTE flags (bits [7:0]):
 *   bit 7   = Cacheable (C)
 *   bit 6   = Modified (M)
 *   bit 5   = Referenced (R)
 *   bit 4:2 = Access Permissions (ACC): 7 = Privileged Read/Write/Execute
 *   bit 1:0 = Entry Type (ET): 2 = PTE
 *
 * Uncached RWX: 0x00 | 0x40 | 0x20 | 0x1C | 0x02 = 0x7E
 * Cached RWX:   0x80 | 0x40 | 0x20 | 0x1C | 0x02 = 0xFE
 */
#define SRMMU_ET_INVALID   0x00u
#define SRMMU_ET_PTD       0x01u
#define SRMMU_ET_PTE       0x02u

#define SRMMU_PTE_UNCACHED 0x7Eu  /* C=0, M=1, R=1, ACC=7, ET=2 */
#define SRMMU_PTE_CACHED   0xFEu  /* C=1, M=1, R=1, ACC=7, ET=2 */

/*
 * Context Table: 256 entries x 4 bytes = 1024 bytes (aligned to 4096 bytes).
 * Level-1 Page Table: 256 entries x 4 bytes = 1024 bytes (aligned to 4096 bytes).
 */
static u32 s_ctx_table[256] __attribute__((aligned(4096)));
static u32 s_level1_pt[256] __attribute__((aligned(4096)));

/* Flush whole TLB via ASI 0x18 */
static inline void srmmu_tlb_flush_all(void)
{
    __asm__ volatile("sta %%g0, [%0] 0x18" :: "r"(0x400u) : "memory");
}

/* MMU register access via ASI 0x19 */
static inline u32 srmmu_read_reg(u32 offset)
{
    u32 val = 0;
    __asm__ volatile("lda [%1] 0x19, %0" : "=r"(val) : "r"(offset) : "memory");
    return val;
}

static inline void srmmu_write_reg(u32 offset, u32 val)
{
    __asm__ volatile("sta %0, [%1] 0x19" :: "r"(val), "r"(offset) : "memory");
}

void npu_mmu_init_identity(void)
{
    u32 i;

    /* 1. Clear Level 1 Page Table (all invalid initially) */
    for (i = 0; i < 256u; i++)
        s_level1_pt[i] = SRMMU_ET_INVALID;

    /* 2. Map MMIO + NNCMX: 0x20000000..0x3FFFFFFF (32 x 16 MB), uncached */
    for (i = 0x20u; i <= 0x3Fu; i++)
        s_level1_pt[i] = ((i << 24) >> 4) | SRMMU_PTE_UNCACHED;

    /* 3. Map FW runtime & DDR: 0x84000000..0x88FFFFFF (5 x 16 MB), cached */
    for (i = 0x84u; i <= 0x88u; i++)
        s_level1_pt[i] = ((i << 24) >> 4) | SRMMU_PTE_CACHED;

    /* 4. Map Shared DDR (fw_log, IPC): 0x90000000..0x9FFFFFFF (16 x 16 MB), uncached */
    for (i = 0x90u; i <= 0x9Fu; i++)
        s_level1_pt[i] = ((i << 24) >> 4) | SRMMU_PTE_UNCACHED;

    /*
     * 5. Test Virtual Alias:
     * Map VA 0x10000000..0x10FFFFFF (16 MB) -> PA 0x2E000000 (NNCMX Tile 0), uncached.
     */
    s_level1_pt[0x10u] = (0x2E000000u >> 4) | SRMMU_PTE_UNCACHED;

    /* 6. Context Table setup: Context 0 points to Level 1 Page Table */
    s_ctx_table[0] = (((u32)(uintptr_t)s_level1_pt) >> 4) | SRMMU_ET_PTD;
    for (i = 1; i < 256u; i++)
        s_ctx_table[i] = SRMMU_ET_INVALID;

    /* 7. Ensure page tables are completely flushed out of caches */
    npu_l1_dcache_flush();
    npu_l2c_flush_all();

    /* 8. Program CTPR (Context Table Pointer) and CTR (Context Register) */
    srmmu_write_reg(NPU_SRMMU_CTPR, ((u32)(uintptr_t)s_ctx_table) >> 4);
    srmmu_write_reg(NPU_SRMMU_CTR, 0u);

    /* 9. Initial TLB flush */
    srmmu_tlb_flush_all();
}

void npu_mmu_enable(void)
{
    srmmu_tlb_flush_all();
    u32 ctrl = srmmu_read_reg(NPU_SRMMU_CTRL);
    ctrl |= NPU_SRMMU_CTRL_E;
    srmmu_write_reg(NPU_SRMMU_CTRL, ctrl);
    __asm__ volatile("nop; nop; nop" ::: "memory");
}

void npu_mmu_disable(void)
{
    u32 ctrl = srmmu_read_reg(NPU_SRMMU_CTRL);
    ctrl &= ~NPU_SRMMU_CTRL_E;
    srmmu_write_reg(NPU_SRMMU_CTRL, ctrl);
    __asm__ volatile("nop; nop; nop" ::: "memory");
    srmmu_tlb_flush_all();
}

u32 npu_mmu_read_ctrl(void)
{
    return srmmu_read_reg(NPU_SRMMU_CTRL);
}

u32 npu_mmu_read_sfsr(void)
{
    return srmmu_read_reg(NPU_SRMMU_SFSR);
}

u32 npu_mmu_read_sfar(void)
{
    return srmmu_read_reg(NPU_SRMMU_SFAR);
}
