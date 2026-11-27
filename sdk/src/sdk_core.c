/*
 * sdk_core.c — top-level npu_init(), npu_poll(), and npu_safe_read32().
 */
#include "npu3720_sdk.h"

extern volatile u32 g_fault_skipped;

void npu_init(const char *fw_name)
{
    npu_log_init(fw_name ? fw_name : "NPU3720-SDK");
    npu_ipc_init();
    npu_timer_init();
    npu_ipc_send_boot();
}

void npu_poll(void)
{
    npu_jsm_poll();
}

int npu_safe_read32(u32 addr, u32 *out)
{
    u32 val;
    g_fault_skipped = 0;
    __asm__ volatile("" ::: "memory");
    val = mmio_read32(addr);
    __asm__ volatile("" ::: "memory");
    if (g_fault_skipped)
        return -1;
    if (out)
        *out = val;
    return 0;
}
