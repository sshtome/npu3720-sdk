/*
 * sdk_core.c — top-level npu_init(), npu_poll(), and npu_safe_read32().
 */
#include "npu3720_sdk.h"

extern volatile u32 g_fault_skipped;

void npu_init(const char *fw_name)
{
    int mtrr_rc = npu_l2c_configure_shared_uncached();

    npu_log_init(fw_name ? fw_name : "NPU3720-SDK");
    npu_log_puts("l2c: init_ctrl=0x");
    npu_log_hex(npu_l2c_ctrl_initial());
    npu_log_puts(" init_stat=0x");
    npu_log_hex(npu_l2c_stat_initial());
    npu_log_puts(" ctrl=0x");
    npu_log_hex(mmio_read32(NPU_GRLIB_L2C_CTRL));
    npu_log_puts(" mtrr0=0x");
    npu_log_hex(mmio_read32(NPU_GRLIB_L2C_MTRR(0u)));
    npu_log_puts(" mtrr=");
    npu_log_dec(mtrr_rc == 0 ? (u32)npu_l2c_is_mtrr_active() : 0u);
    npu_log_puts(" init_ccr=0x");
    npu_log_hex(npu_l1_ccr_initial());
    npu_log_puts(" ccr=0x");
    npu_log_hex(npu_l1_ccr_read());
    npu_log_puts("\n");

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
