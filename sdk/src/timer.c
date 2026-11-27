/*
 * timer.c — free-running TIM counter (0x2602009C) and uptime calculation.
 */
#include "npu3720_sdk.h"

static u32 timer_t0;
static u32 timer_freq_hz;
static int timer_counts_down;

void npu_timer_init(void)
{
    const struct npu_boot_params *bp =
        (const struct npu_boot_params *)NPU_BOOT_PARAMS_ADDR;
    u32 a, b, i;

    a = mmio_read32(NPU_TIM_WATCHDOG);
    for (i = 0; i < 64u; i++)
        __asm__ volatile("nop");
    b = mmio_read32(NPU_TIM_WATCHDOG);

    timer_t0 = a;
    timer_counts_down = (b < a) ? 1 : 0;

    if (bp->perf_clk_frequency != 0)
        timer_freq_hz = bp->perf_clk_frequency;
    else if (bp->frequency != 0)
        timer_freq_hz = bp->frequency;
    else
        timer_freq_hz = 38400000u; /* 38.4 MHz fallback */
}

u32 npu_ticks(void)
{
    return mmio_read32(NPU_TIM_WATCHDOG);
}

u32 npu_timer_freq_hz(void)
{
    return timer_freq_hz;
}

u32 npu_uptime_ms(void)
{
    u32 now = mmio_read32(NPU_TIM_WATCHDOG);
    u32 elapsed = timer_counts_down ? (timer_t0 - now) : (now - timer_t0);
    u32 freq_khz = timer_freq_hz / 1000u;
    if (freq_khz == 0)
        return 0;
    return elapsed / freq_khz;
}
