/*
 * leonnn.c — Intel NPU 3720 LeonNN (Core 1) startup, mailbox & IPC driver.
 */
#include "npu3720_sdk.h"

/* Mailbox located at base of NNCMX Tile 0 (0x2E000000) */
#define LNN_MBOX_ADDR   NPU_CMX_BASE
#define LNN_MAGIC_VAL   0x4C4E4E31u  /* "LNN1" */

static volatile u32 lnn_rx_ipc_count = 0;
static volatile u32 lnn_last_rx_ipc_word = 0;
volatile u32 g_lnn_fault_skipped = 0;

struct npu_lnn_mailbox *npu_leonnn_get_mailbox(void)
{
    return (struct npu_lnn_mailbox *)LNN_MBOX_ADDR;
}

int npu_leonnn_start(u32 entry_addr)
{
    struct npu_lnn_mailbox *mb = npu_leonnn_get_mailbox();
    u32 val;

    if ((entry_addr & 0xFFFu) != 0)
        return -1;

    /* 1. Ensure NCE Spine is powered */
    if (npu_cpr_init_nce_spine() != 0)
        return -2;

    /* 2. Clear mailbox in CMX */
    mb->magic = 0;
    mb->asr17 = 0;
    mb->counter = 0;
    mb->status = 0;
    mb->last_rx_ipc = 0;
    mb->srmmu_ctrl = 0;
    mb->ccr = 0;
    mb->fault_flag = 0;

    /* 3. Flush L1D and L2C so LeonNN fetches fresh code */
    npu_l1_dcache_flush();
    npu_l2c_flush_all();

    /* 4. Enable dev 4 (dom 2, bit 1 = 0x2) bus/dep and dev 3 (dom 3, bit 0 = 0x1) */
    mmio_write32(NPU_NCE_CPR_CLK_SET, (1u << 1) | (1u << 0));

    /* Reset dev 4 (bus/dep): assert then de-assert */
    mmio_write32(NPU_NCE_CPR_RST_CLR, (1u << 1));
    for (volatile int i = 0; i < 50; i++)
        __asm__ volatile("nop");
    mmio_write32(NPU_NCE_CPR_RST_SET, (1u << 1));

    /* 5. Set target entry address in 0x2F008000 before LeonNN reset pulse */
    mmio_write32(NPU_NCE_CPR_LEON_NN_VEC, entry_addr);

    /* 6. Reset dev 3 (LeonNN): assert then de-assert */
    mmio_write32(NPU_NCE_CPR_RST_CLR, (1u << 0));
    for (volatile int i = 0; i < 50; i++)
        __asm__ volatile("nop");
    mmio_write32(NPU_NCE_CPR_RST_SET, (1u << 0));

    /* 7. Pulse RUN/RSTRUN: val |= 6, then (val & ~4) | 2 */
    val = mmio_read32(NPU_NCE_CPR_LEON_NN_VEC);
    val |= 0x6u;
    mmio_write32(NPU_NCE_CPR_LEON_NN_VEC, val);
    for (volatile int i = 0; i < 50; i++)
        __asm__ volatile("nop");

    val = (val & ~0x4u) | 0x2u;
    mmio_write32(NPU_NCE_CPR_LEON_NN_VEC, val);

    return 0;
}

void npu_leonnn_stop(void)
{
    struct npu_lnn_mailbox *mb = npu_leonnn_get_mailbox();
    u32 val;

    mb->status = 2; /* Tell LeonNN to stop */

    val = mmio_read32(NPU_NCE_CPR_LEON_NN_VEC);
    val &= ~0x2u; /* Clear RUN bit */
    mmio_write32(NPU_NCE_CPR_LEON_NN_VEC, val);
}

int npu_leonnn_is_running(void)
{
    return (mmio_read32(NPU_NCE_CPR_LEON_NN_VEC) & 0x2u) ? 1 : 0;
}

void npu_leonnn_send_ipc(u32 word)
{
    /* LRT -> LNN TIM_IPC_FIFO push: (word & ~0x3F) | 5 */
    u32 desc = (word & ~0x3Fu) | 5u;
    mmio_write32(NPU_LNN_FIFO_WRITE, desc);
}

void npu_lnn_on_ipc_word(u32 word)
{
    lnn_last_rx_ipc_word = word;
    lnn_rx_ipc_count++;
}

u32 npu_lnn_get_rx_ipc_count(void)
{
    return lnn_rx_ipc_count;
}

u32 npu_lnn_get_last_rx_ipc_word(void)
{
    return lnn_last_rx_ipc_word;
}

/*
 * C entry for LeonNN (Core 1).
 * Compiled with -msoft-float, running on Core 1 stack.
 */
void npu_lnn_main(void) __attribute__((section(".text.lnn")));

void npu_lnn_main(void)
{
    struct npu_lnn_mailbox *mb = (struct npu_lnn_mailbox *)LNN_MBOX_ADDR;
    u32 asr17_val;
    u32 mmu_val = 0;
    u32 ccr_val = 0;

    __asm__ volatile("rd %%asr17, %0" : "=r"(asr17_val));

    mb->asr17 = asr17_val;
    mb->magic = LNN_MAGIC_VAL;
    mb->fault_flag = 0;

    /* Safe probe of ASI 0x19 (SRMMU CTRL @ 0x000) on LeonNN */
    g_lnn_fault_skipped = 0;
    __asm__ volatile("" ::: "memory");
    __asm__ volatile("lda [%1] 0x19, %0" : "=r"(mmu_val) : "r"(0u) : "memory");
    __asm__ volatile("" ::: "memory");
    if (g_lnn_fault_skipped) {
        mb->srmmu_ctrl = 0xDEADFA09u;
        mb->fault_flag |= 1u;
    } else {
        mb->srmmu_ctrl = mmu_val;
    }

    /* Safe probe of ASI 0x02 (L1 CCR @ 0x000) on LeonNN */
    g_lnn_fault_skipped = 0;
    __asm__ volatile("" ::: "memory");
    __asm__ volatile("lda [%1] 0x02, %0" : "=r"(ccr_val) : "r"(0u) : "memory");
    __asm__ volatile("" ::: "memory");
    if (g_lnn_fault_skipped) {
        mb->ccr = 0xDEADFA09u;
        mb->fault_flag |= 2u;
    } else {
        mb->ccr = ccr_val;
    }

    mb->status = 1;

    /* Main service loop for LeonNN */
    while (mb->status == 1) {
        mb->counter++;

        /* Poll LNN TIM_IPC_FIFO (0x2F0020FC fill count) */
        u32 stat = mmio_read32(NPU_LNN_FIFO_STAT);
        u32 fill = (stat >> 16) & 0xFFu;
        if (fill != 0u) {
            u32 word = mmio_read32(NPU_LNN_FIFO_ATM);
            mb->last_rx_ipc = word;
            /* Acknowledge in LNN ICB */
            mmio_write32(NPU_LNN_FIFO_ACK, 0x1000u);
            (void)mmio_read32(NPU_LNN_ICB_BASE);

            /* Respond back to LRT TIM_IPC_FIFO (0x260200F0) with NPU_LNN_IPC_TAG */
            u32 resp = NPU_LNN_IPC_TAG | (word & 0x00FFFFFFu);
            mmio_write32(NPU_H2D_FIFO_WRITE, (resp & ~0x3Fu) | 7u);
        }

        /* Brief pause */
        for (volatile int i = 0; i < 50; i++)
            __asm__ volatile("nop");
    }

    mb->status = 3; /* Stopped */
    for (;;)
        __asm__ volatile("nop");
}
