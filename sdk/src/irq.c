/*
 * irq.c — 128-line Interrupt Control Block (ICB @ 0x26000000) driver for LeonRT.
 */
#include "npu3720_sdk.h"

static npu_irq_handler_t irq_handlers[NPU_ICB_NUM_IRQS];
static void *irq_ctx[NPU_ICB_NUM_IRQS];
static volatile u32 irq_counts[NPU_ICB_NUM_IRQS];
static volatile u32 spurious_count;

void npu_irq_init(void)
{
    u32 i;

    for (i = 0u; i < 4u; i++) {
        mmio_write32(NPU_ICB_DISABLE(i), 0xFFFFFFFFu);
        mmio_write32(NPU_ICB_CLEAR(i), 0xFFFFFFFFu);
    }
    (void)mmio_read32(NPU_ICB_BASE);

    for (i = 0u; i < NPU_ICB_NUM_IRQS; i++) {
        irq_handlers[i] = NULL;
        irq_ctx[i] = NULL;
        irq_counts[i] = 0u;
    }
    spurious_count = 0u;

    /* Drain any pending latched ACK IDs */
    for (i = 0u; i < NPU_ICB_NUM_IRQS; i++) {
        if (mmio_read32(NPU_ICB_ACK) >= NPU_ICB_NUM_IRQS)
            break;
    }
}

void npu_irq_register(u32 irq, npu_irq_handler_t handler, void *ctx)
{
    if (irq >= NPU_ICB_NUM_IRQS)
        return;
    irq_handlers[irq] = handler;
    irq_ctx[irq] = ctx;
}

void npu_irq_clear(u32 irq)
{
    u32 bank, bit;
    if (irq >= NPU_ICB_NUM_IRQS)
        return;
    bank = irq >> 5;
    bit = 1u << (irq & 31u);
    mmio_write32(NPU_ICB_CLEAR(bank), bit);
    (void)mmio_read32(NPU_ICB_BASE);
}

void npu_irq_enable(u32 irq, u32 prio, int edge)
{
    u32 bank, bit, en;
    if (irq >= NPU_ICB_NUM_IRQS)
        return;

    bank = irq >> 5;
    bit = 1u << (irq & 31u);

    mmio_write32(NPU_ICB_CLEAR(bank), bit);
    mmio_write32(NPU_ICB_CFG(irq), ((prio & 0xFu) << 2) | (edge ? 2u : 0u));
    en = mmio_read32(NPU_ICB_ENABLE(bank));
    mmio_write32(NPU_ICB_ENABLE(bank), en | bit);
    (void)mmio_read32(NPU_ICB_BASE);
}

void npu_irq_disable(u32 irq)
{
    u32 bank, bit;
    if (irq >= NPU_ICB_NUM_IRQS)
        return;

    bank = irq >> 5;
    bit = 1u << (irq & 31u);
    mmio_write32(NPU_ICB_DISABLE(bank), bit);
    (void)mmio_read32(NPU_ICB_BASE);
}

void npu_irq_force(u32 icb_base, u32 irq)
{
    if (irq >= NPU_ICB_NUM_IRQS)
        return;
    u32 base = icb_base ? icb_base : NPU_ICB_BASE;
    u32 bank = irq >> 5;
    u32 bit  = 1u << (irq & 31u);
    mmio_write32(base + 0x250u + 4u * bank, bit);
    (void)mmio_read32(base);
}

u32 npu_irq_count(u32 irq)
{
    if (irq >= NPU_ICB_NUM_IRQS)
        return 0u;
    return irq_counts[irq];
}

u32 npu_irq_spurious_count(void)
{
    return spurious_count;
}

void npu_irq_global_enable(void)
{
    u32 psr;
    __asm__ volatile(
        "rd  %%psr, %0\n\t"
        "andn %0, 0xF00, %0\n\t"
        "wr  %%g0, %0, %%psr\n\t"
        "nop; nop; nop"
        : "=&r"(psr) :: "memory"
    );
}

void npu_irq_global_disable(void)
{
    u32 psr;
    __asm__ volatile(
        "rd  %%psr, %0\n\t"
        "or  %0, 0xF00, %0\n\t"
        "wr  %%g0, %0, %%psr\n\t"
        "nop; nop; nop"
        : "=&r"(psr) :: "memory"
    );
}

u32 npu_irq_save(void)
{
    u32 old_psr, new_psr;
    __asm__ volatile(
        "rd  %%psr, %0\n\t"
        "or  %0, 0xF00, %1\n\t"
        "wr  %%g0, %1, %%psr\n\t"
        "nop; nop; nop"
        : "=&r"(old_psr), "=&r"(new_psr) :: "memory"
    );
    return old_psr;
}

/*
 * Invariant: npu_irq_restore() performs a read-modify-write on %psr touching
 * ONLY bits [11:8] (PIL). It preserves the current CWP bits [4:0], preventing
 * register window corruption across deep call stacks or interrupt returns.
 */
void npu_irq_restore(u32 psr)
{
    u32 cur, pil;
    __asm__ volatile(
        "rd   %%psr, %0\n\t"
        "andn %0, 0xF00, %0\n\t"
        "and  %2, 0xF00, %1\n\t"
        "or   %0, %1, %0\n\t"
        "wr   %%g0, %0, %%psr\n\t"
        "nop; nop; nop"
        : "=&r"(cur), "=&r"(pil)
        : "r"(psr)
        : "memory"
    );
}

/* Called from crt0.S irq_trampoline with ET=1, PIL=15. */
void npu_irq_dispatch(u32 tt)
{
    u32 irq = mmio_read32(NPU_ICB_ACK);
    (void)tt;

    if (irq >= NPU_ICB_NUM_IRQS) {
        spurious_count++;
        return;
    }

    irq_counts[irq]++;
    if (irq_handlers[irq])
        irq_handlers[irq](irq, irq_ctx[irq]);

    /* W1C pending bit in ICB_CLEAR + read barrier on ICB_BASE */
    mmio_write32(NPU_ICB_CLEAR(irq >> 5), 1u << (irq & 31u));
    (void)mmio_read32(NPU_ICB_BASE);
}
