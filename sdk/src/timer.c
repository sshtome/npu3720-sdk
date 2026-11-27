/*
 * timer.c — 64-bit TIM_PERF_FREE_CNT (0x26029000) and uptime calculation.
 */
#include "npu3720_sdk.h"

static u32 timer_last_lo;
static u32 timer_last_hi;
static u32 timer_rem_ticks;
static u32 timer_uptime_ms;
static u32 timer_freq_hz;

void npu_timer_init(void)
{
    const struct npu_boot_params *bp =
        (const struct npu_boot_params *)NPU_BOOT_PARAMS_ADDR;

    mmio_write32(NPU_TIM_PERF_FRC_EN, 1u);
    npu_ticks64(&timer_last_lo, &timer_last_hi);
    timer_rem_ticks = 0u;
    timer_uptime_ms = 0u;

    timer_freq_hz = (bp->perf_clk_frequency != 0u)
                  ? bp->perf_clk_frequency
                  : NPU_TIM_DEFAULT_FREQ_HZ;
}

u32 npu_ticks(void)
{
    return mmio_read32(NPU_TIM_PERF_FREE_CNT_LO);
}

void npu_ticks64(u32 *lo, u32 *hi)
{
    u32 h1, l, h2;

    do {
        h1 = mmio_read32(NPU_TIM_PERF_FREE_CNT_HI);
        l  = mmio_read32(NPU_TIM_PERF_FREE_CNT_LO);
        h2 = mmio_read32(NPU_TIM_PERF_FREE_CNT_HI);
    } while (h1 != h2);

    if (lo)
        *lo = l;
    if (hi)
        *hi = h2;
}

u32 npu_timer_freq_hz(void)
{
    return timer_freq_hz;
}

u32 npu_uptime_ms(void)
{
    u32 ticks_per_ms = timer_freq_hz / 1000u;
    u32 lo, hi, d_lo, d_hi;

    if (ticks_per_ms == 0u)
        return 0u;

    npu_ticks64(&lo, &hi);
    d_lo = lo - timer_last_lo;
    d_hi = hi - timer_last_hi;
    if (lo < timer_last_lo)
        d_hi--;

    timer_last_lo = lo;
    timer_last_hi = hi;

    if (d_hi > 0u) {
        u32 wrap_ms = 0xFFFFFFFFu / ticks_per_ms;
        u32 wrap_rem = (0xFFFFFFFFu % ticks_per_ms) + 1u;

        if (wrap_rem >= ticks_per_ms) {
            wrap_ms++;
            wrap_rem -= ticks_per_ms;
        }
        while (d_hi > 0u) {
            timer_uptime_ms += wrap_ms;
            timer_rem_ticks += wrap_rem;
            if (timer_rem_ticks >= ticks_per_ms) {
                timer_uptime_ms++;
                timer_rem_ticks -= ticks_per_ms;
            }
            d_hi--;
        }
    }

    timer_uptime_ms += d_lo / ticks_per_ms;
    timer_rem_ticks += d_lo % ticks_per_ms;
    if (timer_rem_ticks >= ticks_per_ms) {
        timer_uptime_ms++;
        timer_rem_ticks -= ticks_per_ms;
    }

    return timer_uptime_ms;
}
