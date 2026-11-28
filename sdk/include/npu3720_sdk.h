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
void npu_ipc_enable_irq(void);
void npu_ipc_send(u32 data_addr, u32 data_size, u16 channel);
void npu_ipc_send_boot(void);
void npu_jsm_poll(void);
u32  npu_jsm_count(void);
u32  npu_jsm_last_type(void);

/* Timer (TIM_PERF_FREE_CNT @ 0x26029000, Timer0 Tick @ 0x2602800C) */
void npu_timer_init(void);
u32  npu_ticks(void);
void npu_ticks64(u32 *lo, u32 *hi);
u32  npu_timer_freq_hz(void);
u32  npu_uptime_ms(void);
void npu_timer_tick_start(u32 period_us);
void npu_timer_tick_ack(void);

/* Interrupt Controller (128-IRQ ICB @ 0x26000000) */
typedef void (*npu_irq_handler_t)(u32 irq, void *ctx);
void npu_irq_init(void);
void npu_irq_register(u32 irq, npu_irq_handler_t handler, void *ctx);
void npu_irq_enable(u32 irq, u32 prio, int edge);
void npu_irq_disable(u32 irq);
void npu_irq_clear(u32 irq);
u32  npu_irq_count(u32 irq);
u32  npu_irq_spurious_count(void);
void npu_irq_global_enable(void);
void npu_irq_global_disable(void);
u32  npu_irq_save(void);
void npu_irq_restore(u32 psr);

/* Safe memory read (protected by trap 0x09 fault_skip) */
int  npu_safe_read32(u32 addr, u32 *out);

/* Power, Clocks & Reset (CPR) */
int  npu_cpr_init_nce_spine(void);
int  npu_cpr_power_tile0(void);
int  npu_cpr_power_tile1(void);
int  npu_cpr_power_compute_all(void);
u32  npu_cpr_punit_status(void);

/* NNCMX SRAM (0x2E000000..0x2E3FFFFF, 2x2 MB) */
int  npu_cmx_probe(void);
int  npu_cmx_test_rw_integrity(void);
void npu_cmx_benchmark(u32 *cmx_write_mbs, u32 *cmx_read_mbs, u32 *ddr_write_mbs, u32 *ddr_read_mbs);

/* LeonNN (Core 1, %asr17[31:28] == 2) */
struct npu_lnn_mailbox {
    volatile u32 magic;       /* 0x4C4E4E31 ("LNN1") */
    volatile u32 asr17;       /* Core 1 %asr17 */
    volatile u32 counter;     /* Incremented by Core 1 */
    volatile u32 status;      /* 1 = booted/ready, 2 = stopping */
    volatile u32 last_rx_ipc; /* Last word received via TIM_IPC_FIFO */
};

extern void npu_lnn_start_entry(void);
int  npu_leonnn_start(u32 entry_addr);
void npu_leonnn_stop(void);
int  npu_leonnn_is_running(void);
struct npu_lnn_mailbox *npu_leonnn_get_mailbox(void);
void npu_leonnn_send_ipc(u32 word);
void npu_lnn_on_ipc_word(u32 word);
u32  npu_lnn_get_rx_ipc_count(void);
u32  npu_lnn_get_last_rx_ipc_word(void);

/* Bidirectional console (RX via 0xCCxxxxxx H2D words, TX via fw_log) */
typedef void (*npu_console_cb_t)(const char *line);
void npu_console_register(npu_console_cb_t cb);
void npu_console_rx_word(u32 word);

/* NNDMA (HglDmaCreateDescriptor / HglDmaEnable RE, sparcel ELF) */
void npu_dma_init(void);
int  npu_dma_copy_1d(unsigned tile, u32 dst, u32 src, u32 len);

#endif /* NPU3720_SDK_H */
