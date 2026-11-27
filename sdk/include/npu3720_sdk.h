/*
 * npu3720_sdk.h — public bare-metal C API for Intel NPU 3720 (LeonRT).
 */
#ifndef NPU3720_SDK_H
#define NPU3720_SDK_H

#include "npu3720_hw.h"

/* Lifecycle */
void npu_init(const char *fw_name);
void npu_poll(void);

/* Cache / GRLIB L2C (0x27000000) */
u32  npu_l1_ccr_read(void);
void npu_l1_ccr_write(u32 val);
u32  npu_l1_ccr_initial(void);
void npu_l1_dcache_flush(void);
void npu_l1_set_flush_enabled(int en);
int  npu_l1_is_flush_enabled(void);
void npu_l2c_clean_range(u32 addr, u32 size);
void npu_l2c_flush_all(void);
void npu_l2c_set_mtrr(u32 idx, u32 addr, u32 mask, u32 access_ctrl, int wp, int enable);
int  npu_l2c_configure_shared_uncached(void);
int  npu_l2c_is_mtrr_active(void);
u32  npu_l2c_ctrl_initial(void);
u32  npu_l2c_stat_initial(void);

/* Logging (fw_log @ 0x90000000) */
void npu_log_init(const char *name);
void npu_log_putc(char c);
void npu_log_puts(const char *s);
void npu_log_hex(u32 v);
void npu_log_dec(u32 v);
void npu_log_flush(void);

/* IPC & JSM */
void npu_ipc_init(void);
void npu_ipc_send(u32 data_addr, u32 data_size, u16 channel);
void npu_ipc_send_boot(void);
void npu_jsm_poll(void);
u32  npu_jsm_count(void);
u32  npu_jsm_last_type(void);

/* Timer (TIM_PERF_FREE_CNT @ 0x26029000) */
void npu_timer_init(void);
u32  npu_ticks(void);
void npu_ticks64(u32 *lo, u32 *hi);
u32  npu_timer_freq_hz(void);
u32  npu_uptime_ms(void);

/* Safe memory read (protected by trap 0x09 fault_skip) */
int  npu_safe_read32(u32 addr, u32 *out);

/* Bidirectional console (RX via 0xCCxxxxxx H2D words, TX via fw_log) */
typedef void (*npu_console_cb_t)(const char *line);
void npu_console_register(npu_console_cb_t cb);
void npu_console_rx_word(u32 word);

#endif /* NPU3720_SDK_H */
