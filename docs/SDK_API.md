# NPU 3720 SDK C API Reference

Include `npu3720_sdk.h` (which includes `npu3720_hw.h`) and implement `void kmain(void)`.

## Lifecycle

```c
void npu_init(const char *fw_name);
void npu_poll(void);
```

- `npu_init(fw_name)` configures GRLIB L2C MTRR (`0x9000FFE1`), initializes `fw_log` (`0x90000000`), enables `TIM_PERF_FREE_CNT` (`0x26029000`), initializes IPC/JSM buffers, and sends the `BOOT` IPC message.
- `npu_poll()` polls/handles `NPU_H2D_FIFO_STAT`, dispatches `0xCCxxxxxx` console words to `npu_console_rx_word()`, and responds to JSM messages (`0x1113`, `0x1104`).

## Cache & GRLIB L2C (`0x27000000`)

```c
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
```

## Logging (`fw_log`)

```c
void npu_log_init(const char *name);
void npu_log_putc(char c);
void npu_log_puts(const char *s);
void npu_log_hex(u32 v);
void npu_log_dec(u32 v);
void npu_log_flush(void);
```

`npu_log_puts()`, `npu_log_hex()`, and `npu_log_dec()` flush L1D (when enabled) and clean L2C automatically if MTRR uncached mapping is inactive; with `MTRR[0] = 0x9000FFE1` active, `fw_log` is coherent without per-write `l2c_clean_range()`. Output appears in `/sys/kernel/debug/accel/0000:00:0b.0/fw_log`.

## IPC & JSM

```c
void npu_ipc_init(void);
void npu_ipc_enable_irq(void);
void npu_ipc_send(u32 data_addr, u32 data_size, u16 channel);
void npu_ipc_send_boot(void);
void npu_jsm_poll(void);
u32  npu_jsm_count(void);
u32  npu_jsm_last_type(void);
```

## Timer & Safe Memory Read

```c
void npu_timer_init(void);
u32  npu_ticks(void);
void npu_ticks64(u32 *lo, u32 *hi);
u32  npu_timer_freq_hz(void);
u32  npu_uptime_ms(void);
void npu_timer_tick_start(u32 period_us);
void npu_timer_tick_ack(void);
int  npu_safe_read32(u32 addr, u32 *out);
```

- `npu_ticks()` / `npu_ticks64()` atomically sample the 64-bit `TIM_PERF_FREE_CNT` (`0x26029000/04`, 38.4 MHz) via `ldd`.
- `npu_timer_tick_start(period_us)` arms Timer0 (`0x2602800C..14`) for periodic `IRQ 108` (`npu_timer_tick_ack()` clears `TIM_TICK_STAT`).
- `npu_safe_read32()` clears `g_fault_skipped`, reads `*(volatile u32 *)addr`, and returns `-1` if `trap 0x09` (`fault_skip`) fired, or `0` on success. Probe unmapped AHB holes (`< 0x80000000`, e.g. `0x40000000`); avoid `0xA0000000` which triggers a host TBU MMU fault.

## Interrupt Controller (`ICB @ 0x26000000`)

```c
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
```

- `npu_irq_save()` / `npu_irq_restore()` mask `PIL=15` and restore only `PIL` bits `[11:8]` in `%psr` so register window calls stay safe.

## Console

```c
typedef void (*npu_console_cb_t)(const char *line);
void npu_console_register(npu_console_cb_t cb);
```

Register a line callback before entering the `npu_poll()` loop. See `examples/shell/main.c` for a complete interactive shell, and `examples/selftest/main.c` for an automated hardware test suite covering L2C/MTRR, register windows, `fault_skip`, timer ticks, and ICB interrupts.

## CPR & Power Management

```c
int npu_cpr_init_nce_spine(void);
int npu_cpr_power_tile0(void);
int npu_cpr_power_tile1(void);
int npu_cpr_power_compute_all(void);
u32 npu_cpr_punit_status(void);
```

- `npu_cpr_init_nce_spine()` enables AON Island 5, unmasks `TOP_NOC_NCE_AXI` (`0x20000160`), and brings up `NCE_CPR_NOC_Q_CTRL` (`0x2F008034`).
- `npu_cpr_power_tile0()` / `npu_cpr_power_tile1()` power on AON Islands 32 and 33, pulse `TILE_AUX_SET` (`0x2F008024`), and enable Tile CPR devices.
- `npu_cpr_punit_status()` returns the active PLL ratio and workpoint mirror from `0x20082020` (e.g. `0x02020016`).

## NNCMX SRAM (4 MB, `0x2E000000`)

```c
int  npu_cmx_probe(void);
int  npu_cmx_test_rw_integrity(void);
void npu_cmx_benchmark(u32 *cmx_w, u32 *cmx_r, u32 *ddr_w, u32 *ddr_r);
```

- `npu_cmx_probe()` verifies that both Tile 0 (`0x2E000000`) and Tile 1 (`0x2E200000`) are accessible.
- `npu_cmx_test_rw_integrity()` validates 4 MB bitwise pattern integrity across all slices.
- `npu_cmx_benchmark()` measures write/read speeds across 64 KB blocks (measured 262 MB/s write on CMX vs 18 MB/s on DDR).

## LeonNN Core 1 (`%asr17[31:28] == 2`) & Dual-Core IPC

```c
struct npu_lnn_mailbox {
    volatile u32 magic;       /* 0x4C4E4E31 ("LNN1") */
    volatile u32 asr17;       /* Core 1 %asr17 */
    volatile u32 counter;     /* Incremented by Core 1 */
    volatile u32 status;      /* 1 = booted/ready, 2 = stopping */
    volatile u32 last_rx_ipc; /* Last word received via TIM_IPC_FIFO */
};

int  npu_leonnn_start(u32 entry_addr);
void npu_leonnn_stop(void);
int  npu_leonnn_is_running(void);
struct npu_lnn_mailbox *npu_leonnn_get_mailbox(void);
void npu_leonnn_send_ipc(u32 word);
u32  npu_lnn_get_rx_ipc_count(void);
u32  npu_lnn_get_last_rx_ipc_word(void);
```

- `npu_leonnn_start(entry_addr)` prepares the runtime, pulses reset, and launches Core 1 at `entry_addr` (must be 4 KB aligned).
- `npu_leonnn_send_ipc(word)` pushes a message to LeonNN's hardware FIFO (`0x2F0020F0`).
- LeonNN responds back to LeonRT's FIFO (`0x260200F0`) tagged with `NPU_LNN_IPC_TAG` (`0xBB000000`).

## Writing a New Application

1. Copy `examples/hello/` to `examples/myapp/`.
2. Update `vpu_37xx_hello.bin` target name in `Makefile` if desired.
3. Add `examples/myapp` to `EXAMPLES` in the root `Makefile`.
4. Build with `make` and test with `sudo ./tools/run_test.sh sched1 examples/myapp/vpu_37xx_myapp.bin` (or run `sudo ./tools/run_test.sh selftest` for the full Gate B suite).
