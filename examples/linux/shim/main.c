/* SPDX-License-Identifier: MIT */
#include "romvec.h"

/*
 * NPU 3720 Linux Shim Bootloader.
 *
 * Runs at physical address 0x84803000.
 * 1. Initializes SDK core, L2 cache, MTRR and fw_log in shared DDR.
 * 2. Prints boot banner and hardware info.
 * 3. Builds SRMMU page tables mapping:
 *    - VA 0xF0000000..0xF2FFFFFF -> PA 0x85000000..0x87FFFFFF (cached RWX, 48 MB Linux text/data)
 *    - VA 0x00000000..0x00FFFFFF -> PA 0x85000000 (temporary mirror for early head_32.S)
 *    - VA 0x84000000..0x88FFFFFF -> PA 0x84000000..0x88FFFFFF (cached identity)
 *    - VA 0x20000000..0x3FFFFFFF -> PA 0x20000000..0x3FFFFFFF (uncached identity: MMIO, NNCMX)
 *    - VA 0x90000000..0x9FFFFFFF -> PA 0x90000000..0x9FFFFFFF (uncached identity: fw_log, IPC)
 * 4. Prepares minimal struct linux_romvec with earlycon routed to fw_log.
 * 5. Enables SRMMU and jumps to Linux entry at 0xF0004000 with %o0 = &romvec.
 */

#define LINUX_VIRT_ENTRY 0xF0004000u

/* SRMMU constants */
#define SRMMU_ET_INVALID   0x00u
#define SRMMU_ET_PTD       0x01u
#define SRMMU_ET_PTE       0x02u

#define SRMMU_PTE_UNCACHED 0x7Eu  /* C=0, M=1, R=1, ACC=7, ET=2 */
#define SRMMU_PTE_CACHED   0xFEu  /* C=1, M=1, R=1, ACC=7, ET=2 */

static u32 s_ctx_table[256] __attribute__((aligned(4096)));
static u32 s_level1_pt[256] __attribute__((aligned(4096)));

static inline void shim_srmmu_tlb_flush_all(void)
{
    __asm__ volatile("sta %%g0, [%0] 0x18" :: "r"(0x400u) : "memory");
}

static inline void shim_srmmu_write_reg(u32 offset, u32 val)
{
    __asm__ volatile("sta %0, [%1] 0x19" :: "r"(val), "r"(offset) : "memory");
}

static void setup_srmmu_mappings(void)
{
    u32 i;

    /* 1. Clear Level 1 Page Table */
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
     * 5. Map Linux KERNBASE:
     * VA 0xF0000000..0xF2FFFFFF (48 MB, 3 x 16 MB sections) -> PA 0x85000000..0x87FFFFFF
     */
    s_level1_pt[0xF0u] = (0x85000000u >> 4) | SRMMU_PTE_CACHED;
    s_level1_pt[0xF1u] = (0x86000000u >> 4) | SRMMU_PTE_CACHED;
    s_level1_pt[0xF2u] = (0x87000000u >> 4) | SRMMU_PTE_CACHED;

    /*
     * 6. Map VA 0x00000000..0x00FFFFFF -> PA 0x85000000 (temporary 16 MB mirror)
     * For early SPARC head_32.S boot code that accesses 0x0 before highmem jump.
     */
    s_level1_pt[0x00u] = (0x85000000u >> 4) | SRMMU_PTE_CACHED;

    /* 7. Context Table setup: Context 0 points to Level 1 Page Table */
    s_ctx_table[0] = (((u32)(uintptr_t)s_level1_pt) >> 4) | SRMMU_ET_PTD;
    for (i = 1; i < 256u; i++)
        s_ctx_table[i] = SRMMU_ET_INVALID;

    /* 8. Flush L1 D-cache and L2C */
    npu_l1_dcache_flush();
    npu_l2c_flush_all();

    /* 9. Program CTPR and CTR */
    shim_srmmu_write_reg(NPU_SRMMU_CTPR, ((u32)(uintptr_t)s_ctx_table) >> 4);
    shim_srmmu_write_reg(NPU_SRMMU_CTR, 0u);

    /* 10. Flush TLB */
    shim_srmmu_tlb_flush_all();
}

static void enable_srmmu_and_jump(u32 entry_vaddr, void *romvec_ptr)
{
    npu_log_puts("[SHIM] Enabling SRMMU and transferring control to Linux...\n");

    /* Flush caches once more before MMU enable */
    npu_l1_dcache_flush();
    npu_l2c_flush_all();

    __asm__ volatile(
        "mov %0, %%o0\n\t"              /* Arg 0: struct linux_romvec * */
        "mov %1, %%g1\n\t"              /* Entry address: 0xF0004000 */
        "set 0x1, %%g2\n\t"             /* MMU Control bit 0 = MMU Enable */
        "sta %%g2, [%%g0] 0x19\n\t"     /* Write MMU Control Register (ASI 0x19, offset 0) */
        "nop; nop; nop\n\t"
        "sta %%g0, [%2] 0x18\n\t"       /* Flush TLB (ASI 0x18, 0x400) */
        "nop; nop; nop\n\t"
        "jmp %%g1\n\t"                  /* Jump to Linux entry */
        " nop\n\t"
        :
        : "r"(romvec_ptr), "r"(entry_vaddr), "r"(0x400u)
        : "g1", "g2", "o0", "memory"
    );
}

void kmain(void)
{
    npu_init("NPU3720-LINUX-SHIM");

    npu_log_puts("\n==================================================\n");
    npu_log_puts("   Intel NPU 3720 Linux 6.6 Shim Bootloader       \n");
    npu_log_puts("==================================================\n");

    u32 psr_val = 0, asr17_val = 0;
    __asm__ volatile("rd %%psr, %0" : "=r"(psr_val));
    __asm__ volatile("rd %%asr17, %0" : "=r"(asr17_val));

    npu_log_puts("[SHIM] CPU info: LeonRT PSR=0x");
    npu_log_hex(psr_val);
    npu_log_puts(" ASR17=0x");
    npu_log_hex(asr17_val);
    npu_log_puts("\n");

    /* Initialize romvec with command line */
    const char *cmdline = "console=earlyprom earlycon";
    struct linux_romvec *romvec = shim_romvec_init(cmdline);
    npu_log_puts("[SHIM] romvec initialized at 0x");
    npu_log_hex((u32)romvec);
    npu_log_puts(", cmdline: \"");
    npu_log_puts(cmdline);
    npu_log_puts("\"\n");

    /* Setup SRMMU page tables */
    npu_log_puts("[SHIM] Configuring SRMMU Level 1 page tables...\n");
    setup_srmmu_mappings();

    npu_log_puts("[SHIM] KERNBASE 0xF0000000 mapped to PA 0x85000000 (48 MB)\n");
    npu_log_puts("[SHIM] Target kernel entry: 0x");
    npu_log_hex(LINUX_VIRT_ENTRY);
    npu_log_puts("\n");

    /* Transfer control */
    enable_srmmu_and_jump(LINUX_VIRT_ENTRY, romvec);

    /* Should not return */
    for (;;) {
        __asm__ volatile("nop");
    }
}
