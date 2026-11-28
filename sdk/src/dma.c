/*
 * dma.c — NNDMA hardware driver for Intel NPU 3720 (LeonRT bare-metal).
 *
 * Static RE source:
 *   HglDmaCreateDescriptor @ 0x85018254 (sparcel std to +0x10 and +0x18)
 *   DmaJobStart @ 0x8503f28c (std to engine+0x640 Link Agent Kick port)
 *   lnn_DmaLinkAgentGetLastProcessedAddress @ 0x850f1268 (+0x840, +0x940, +0xB40)
 *   HglDmaEnable @ 0x8503dfbc, HglCprResetDevice @ 0x850188dc.
 *
 * Descriptor layout [VERIFIED-CODE]: ctrl=0x01F803FC (LE uint32),
 *   sparcel std %g2, [desc+0x10] -> src_lo @ +0x10, src_hi @ +0x14.
 *   sparcel std %g2, [desc+0x18] -> dst_lo @ +0x18, dst_hi @ +0x1C.
 * Kick port [VERIFIED-CODE]: engine + 0x640 + ch*0x10
 *   std %g2, [engine+0x640] -> word0=desc_phys, word1=(prio<<24)|0x100.
 */

#include "npu3720_hw.h"
#include "npu3720_sdk.h"

/* ── descriptor ─────────────────────────────────────────────────────────── */

struct __attribute__((packed, aligned(64))) vpu_dma_descriptor {
    u32 link_addr;   /* +0x00: next desc phys addr (0 = terminal) */
    u32 flags;       /* +0x04: bit31 = watermark/IRQ trigger */
    u32 ctrl;        /* +0x08: 0x01F803FC — 256-burst, ch=0, no-IRQ, valid */
    u32 reserved0;   /* +0x0C */
    u32 src_lo;      /* +0x10: src addr bits[31:0]  (sparcel lo32) */
    u32 src_hi;      /* +0x14: src addr bits[63:32] (sparcel hi32) */
    u32 dst_lo;      /* +0x18: dst addr bits[31:0]  (sparcel lo32) */
    u32 dst_hi;      /* +0x1C: dst addr bits[63:32] (sparcel hi32) */
    u32 length;      /* +0x20: byte count */
    u8  num_planes;  /* +0x24: 1 = 1D transfer */
    u8  pad[3];      /* +0x25..0x27 */
    u32 src_width;   /* +0x28 */
    u32 dst_width;   /* +0x2C */
    u32 src_stride;  /* +0x30 */
    u32 src_pstride; /* +0x34 */
    u32 dst_stride;  /* +0x38 */
    u32 dst_pstride; /* +0x3C */
};

/*
 * ctrl = 0x01F803FC  [VERIFIED-CODE from HglDmaCreateDescriptor disasm]:
 *   byte[+8] = 0xFC: channel=0, burst_lo = (0x3F << 2)
 *   byte[+9] = 0x03: burst_hi  = (0xFF >> 6)
 *   byte[+A] = 0xF8: default ctrl mask (bit3=watermark=0)
 *   byte[+B] = 0x01: Valid bit
 */
#define DMA_CTRL_1D_NOIRQ  0x01F803FCu

/*
 * Descriptor pool in NNCMX Tile 0, clear of:
 *   LeonRT stack top  @ 0x2E000000 + ~0x1400 (5 KB stack)
 *   LeonNN stack base @ 0x2E001400
 * 0x2E008000 is safely clear of both; 2 descriptors × 64 B = 128 B.
 */
#define DMA_DESC_POOL_BASE  0x2E008000u

/* ── LA kick — exact match of Intel firmware's std %o4,[engine+0x640] ────── */

static inline void dma_kick_enqueue(u32 port, u32 w0, u32 w1)
{
    /*
     * SPARC std requires an even-numbered register pair.
     * Force %o4/%o5 so the compiler emits a single 64-bit store instruction.
     * [VERIFIED-CODE @ 0x8503f5cc]: stock firmware uses std %g2,[%i1] where %i1=engine+0x640.
     */
    register u32 r0 __asm__("o4") = w0;
    register u32 r1 __asm__("o5") = w1;
    __asm__ volatile("std %%o4, [%0]" : : "r"(port), "r"(r0), "r"(r1) : "memory");
}

/* ── tile enable ─────────────────────────────────────────────────────────── */

static void dma_enable_tile(u32 engine, u32 decoder, u32 noc_base)
{
    /* NOC arbitration [VERIFIED-CODE from HglDmaEnable @ 0x8503dfbc] */
    mmio_write32(noc_base + 0x0u, 0x00000003u);
    mmio_write32(noc_base + 0x4u, 0x00000707u);
    mmio_write32(noc_base + 0x8u, 0x00000808u);

    /* Decoder: enable + all-channel mask + clear interrupts */
    mmio_write32(decoder + NPU_DMA_DEC_CTRL,       mmio_read32(decoder + NPU_DMA_DEC_CTRL) | 1u);
    mmio_write32(decoder + NPU_DMA_DEC_CH_MASK,   0x1FFu);
    mmio_write32(decoder + NPU_DMA_DEC_IRQ_STATUS, 0xFFFFFFFFu);

    /* Engine: enable + all-channel mask + clear interrupts */
    mmio_write32(engine + NPU_DMA_ENG_CTRL,       mmio_read32(engine + NPU_DMA_ENG_CTRL) | 1u);
    mmio_write32(engine + NPU_DMA_ENG_CH_MASK,   0x1FFu);
    mmio_write32(engine + NPU_DMA_ENG_IRQ_CLR,   0xFFFFFFFFu);
}

/* ── public API ──────────────────────────────────────────────────────────── */

void npu_dma_init(void)
{
    /* NCE Spine (Island 5) already powered by npu_cpr_init_nce_spine(). */

    /*
     * Reset strobe: assert reset via RST_CLR, turn clocks on, release reset via RST_SET.
     * Matches stock HglCprResetDevice @ 0x850188dc sequence.
     */
    mmio_write32(NPU_NCE_CPR_RST_CLR, NPU_CPR_DMA_MASK);
    for (volatile u32 i = 0; i < 40u; i++)
        __asm__ volatile("nop");

    mmio_write32(NPU_NCE_CPR_CLK_SET, NPU_CPR_DMA_MASK);
    for (volatile u32 i = 0; i < 40u; i++)
        __asm__ volatile("nop");

    mmio_write32(NPU_NCE_CPR_RST_SET, NPU_CPR_DMA_MASK);
    for (volatile u32 i = 0; i < 40u; i++)
        __asm__ volatile("nop");

    /*
     * Host Interface SSID map table @ 0x20340000 [VERIFIED-CODE HglHostIfSetSsidMap @ 0x85016604]:
     * The Host Interface tags outgoing PCIe/DDR transactions with the SSID from this table.
     * Set all entries to 0 (SSID 0 = Global Context, where fw->mem 0x84800000.. is mapped).
     * If an entry has SSID 1, the SMMU looks up CDTAB[1] (empty page table) -> Translation Fault.
     */
    for (u32 i = 0; i < NPU_HOST_IF_SSID_MAP_ENTRIES; i++)
        mmio_write32(NPU_HOST_IF_SSID_MAP_BASE + i * 4u, 0u);

    dma_enable_tile(NPU_DMA_ENGINE_T0, NPU_DMA_DECODER_T0, NPU_DMA_NOC_T0);
    dma_enable_tile(NPU_DMA_ENGINE_T1, NPU_DMA_DECODER_T1, NPU_DMA_NOC_T1);

    npu_log_puts("DMA: init OK T0=0x2EF60000 T1=0x2EF90000 ssid_map=0\n");
}

int npu_dma_copy_1d(unsigned tile, u32 dst, u32 src, u32 len)
{
    u32 engine   = tile ? NPU_DMA_ENGINE_T1  : NPU_DMA_ENGINE_T0;
    u32 dec_base = tile ? NPU_DMA_DECODER_T1 : NPU_DMA_DECODER_T0;

    /* DDR coherency: flush L1D & clean L2C if source is in DDR */
    if (src >= 0x80000000u) {
        npu_l1_dcache_flush();
        npu_l2c_flush_all();
    }

    /* Descriptor placed in NNCMX — fully coherent, no cache ops needed. */
    volatile struct vpu_dma_descriptor *d =
        (volatile struct vpu_dma_descriptor *)(DMA_DESC_POOL_BASE + (u32)tile * 64u);

    d->link_addr  = 0u;
    d->flags      = 0u;
    d->ctrl       = DMA_CTRL_1D_NOIRQ;
    d->reserved0  = 0u;
    d->src_lo     = src;
    d->src_hi     = 0u;
    d->dst_lo     = dst;
    d->dst_hi     = 0u;
    d->length     = len;
    d->num_planes = 1u;
    d->pad[0] = d->pad[1] = d->pad[2] = 0u;
    d->src_width  = 0u;
    d->dst_width  = 0u;
    d->src_stride = 0u;
    d->src_pstride = 0u;
    d->dst_stride = 0u;
    d->dst_pstride = 0u;

    u32 desc_phys = (u32)(uintptr_t)d;

    /*
     * Kick via Link Agent 0 port @ engine + 0x640:
     * [VERIFIED-CODE @ 0x8503f5cc]: std %g2, [engine + 0x640 + ch*0x10]
     * word0 = desc_phys, word1 = (prio << 24) | 0x100
     */
    dma_kick_enqueue(engine + 0x640u, desc_phys, 0x00000100u);

    /*
     * Poll completion:
     * Channel 0 busy flag is bit 19 in CH0_BUSY (engine + 0x420).
     * Link Agent 0 last processed descriptor is in LA_LAST (engine + 0x840).
     * Timeout ~200 k iterations (~5 ms at 38.4 MHz).
     */
    u32 timeout = 200000u;
    while (--timeout) {
        if (mmio_read32(engine + 0x840u) == desc_phys)
            break;
        if (!(mmio_read32(engine + NPU_DMA_ENG_CH0_BUSY) & 0x80000u) &&
            (mmio_read32(engine + 0x840u) == desc_phys))
            break;
    }

    if (!timeout) {
        npu_log_puts("DMA: TIMEOUT tile=");
        npu_log_dec(tile);
        npu_log_puts(" ch0_busy=0x");
        npu_log_hex(mmio_read32(engine + NPU_DMA_ENG_CH0_BUSY));
        npu_log_puts(" kick=0x");
        npu_log_hex(mmio_read32(engine + 0x640u));
        npu_log_puts(" last=0x");
        npu_log_hex(mmio_read32(engine + 0x840u));
        npu_log_puts(" next=0x");
        npu_log_hex(mmio_read32(engine + 0x940u));
        npu_log_puts(" la_stat=0x");
        npu_log_hex(mmio_read32(engine + NPU_DMA_ENG_LA_STATUS0));
        npu_log_puts(" dec_irq=0x");
        npu_log_hex(mmio_read32(dec_base + NPU_DMA_DEC_IRQ_STATUS));
        npu_log_puts("\n");
        return -1;
    }

    /* DDR coherency: invalidate caches if destination was in DDR */
    if (dst >= 0x80000000u) {
        npu_l2c_flush_all();
        npu_l1_dcache_flush();
    }

    return 0;
}
