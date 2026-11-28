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

- `npu_ipc_init()` reads ring buffer base addresses from `boot_params` (`0x84800000`).
- `npu_ipc_enable_irq()` enables H2D FIFO incoming message interrupt (`IRQ 12`).
- `npu_ipc_send(data_addr, data_size, channel)` builds an IPC descriptor, performs cache flushes, and enqueues to `D2H_FIFO_WRITE` (`0x200200F0`).
- `npu_ipc_send_boot()` sends the initial `"BOOT"` handshake on channel `0x3FF`.
- `npu_jsm_poll()` pops messages from `H2D_FIFO_ATM`, demuxes console words (`0xCCxxxxxx`), LeonNN IPC (`0xBBxxxxxx`), and responds to JSM requests (`0x1113`, `0x1104`).
- `npu_jsm_count()` and `npu_jsm_last_type()` return telemetry statistics on processed JSM requests.

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
void npu_irq_force(u32 icb_base, u32 irq);
```

- `npu_irq_save()` / `npu_irq_restore()` mask `PIL=15` and restore only `PIL` bits `[11:8]` in `%psr` so register window calls stay safe.
- `npu_irq_force(icb_base, irq)` forces an interrupt via `ICB_FORCE` register (useful for software IRQs and inter-core IPI). If `icb_base` is `0`, `NPU_ICB_BASE` (`0x26000000`) is used.

## OS Primitives & Atomics

```c
u32  npu_syscall_yield(void);
u32  npu_get_syscall_count(void);
u32  npu_atomic_swap(volatile u32 *addr, u32 val);
u8   npu_atomic_ldstub(volatile u8 *addr);
```

- `npu_syscall_yield()` issues software trap `ta 0x10` (`tt = 0x90`), triggering `syscall_trap_handler`, which increments `g_syscall_cnt` and returns magic `0x4F53` (`"OS"`).
- `npu_get_syscall_count()` returns the number of handled system call traps.
- `npu_atomic_swap(addr, val)` executes the SPARC v8 hardware `swap` instruction, atomically writing `val` to `addr` and returning the previous value. Verified in DDR and NNCMX SRAM.
- `npu_atomic_ldstub(addr)` executes the SPARC v8 hardware `ldstub` instruction, atomically setting byte `*addr = 0xFF` and returning the previous byte value.

## SPARC Reference MMU (SRMMU, ASI 0x19)

```c
void npu_mmu_init_identity(void);
void npu_mmu_enable(void);
void npu_mmu_disable(void);
u32  npu_mmu_read_ctrl(void);
u32  npu_mmu_read_sfsr(void);
u32  npu_mmu_read_sfar(void);
```

- `npu_mmu_init_identity()` configures a 256-context table and Level 1 page table for a 16 MB section identity map:
  - MMIO & NNCMX (`0x20000000..0x3FFFFFFF`): uncached (`0x7E`).
  - Firmware DDR (`0x84000000..0x88FFFFFF`): cached (`0xFE`).
  - Shared DDR (`0x90000000..0x9FFFFFFF`): uncached (`0x7E`).
  - Virtual Alias: maps `VA 0x10000000..0x10FFFFFF` to `PA 0x2E000000` (NNCMX Tile 0).
- `npu_mmu_enable()` flushes TLB via ASI `0x18` and sets bit 0 (`E`) in MMU Control Register (`ASI 0x19, 0x000`).
- `npu_mmu_disable()` clears bit 0 (`E`) and flushes TLB.
- `npu_mmu_read_ctrl()`, `npu_mmu_read_sfsr()`, `npu_mmu_read_sfar()` inspect MMU state and fault registers.

## Console

```c
typedef void (*npu_console_cb_t)(const char *line);
void npu_console_register(npu_console_cb_t cb);
void npu_console_rx_word(u32 word);
```

Register a line callback before entering the `npu_poll()` loop. Incoming host words (`0xCCxxxxxx`) are routed to `npu_console_rx_word()`, which decodes 2 ASCII characters from bits `[21:6]` into the line buffer. See `examples/shell/main.c` for a complete interactive shell, and `examples/selftest/main.c` for an automated hardware test suite covering L2C/MTRR, register windows, `fault_skip`, timer ticks, and ICB interrupts.

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
void npu_cmx_benchmark(u32 *cmx_write_mbs, u32 *cmx_read_mbs, u32 *ddr_write_mbs, u32 *ddr_read_mbs);
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
    volatile u32 srmmu_ctrl;  /* Core 1 ASI 0x19 0x000 (or 0xDEADFA09 on fault) */
    volatile u32 ccr;         /* Core 1 ASI 0x02 0x000 (L1 CCR) */
    volatile u32 fault_flag;  /* Bit 0: srmmu faulted, Bit 1: ccr faulted */
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
```

- `npu_leonnn_start(entry_addr)` prepares the runtime, pulses reset, and launches Core 1 at `entry_addr` (must be 4 KB aligned).
- `npu_leonnn_send_ipc(word)` pushes a message to LeonNN's hardware FIFO (`0x2F0020F0`).
- `npu_lnn_on_ipc_word(word)` is called by `npu_jsm_poll()` when LeonNN responds with a `0xBB000000` tagged word.

## NNDMA Engine (`sdk/src/dma.c`)

```c
void npu_dma_init(void);
int  npu_dma_copy_1d(unsigned tile, u32 dst, u32 src, u32 len);
```

- `npu_dma_init()` resets and clock-enables NNDMA engines and decoders across Tile 0 and Tile 1 (`NCE_CPR` mask `0xC00`), configures NOC arbitration (`0x2F004000/6000`), enables all channels (`0x1FF`), and zeroes all 32 Host Interface SSID mapping registers (`0x20340000`) so DMA transactions map to SSID 0 (Global Context, where `fw->mem` `0x84800000..0x88800000` is mapped without SMMU faults).
- `npu_dma_copy_1d(tile, dst, src, len)` performs a synchronous 1D transfer using a 64-byte descriptor in NNCMX (`DMA_DESC_POOL_BASE` `0x2E008000`). Handles L1D/L2C cache flushes for DDR addresses, kicks Link Agent 0 via `std %o4, [%0]` at `engine + 0x640`, and polls completion at `engine + 0x840`. Returns `0` on success or `-1` on timeout.
- LeonNN responds back to LeonRT's FIFO (`0x260200F0`) tagged with `NPU_LNN_IPC_TAG` (`0xBB000000`).

## Writing a New Application

1. Copy `examples/hello/` to `examples/myapp/`.
2. Update `vpu_37xx_hello.bin` target name in `Makefile` if desired.
3. Add `examples/myapp` to `EXAMPLES` in the root `Makefile`.
4. Build with `make` and test with `sudo ./tools/run_test.sh sched1 examples/myapp/vpu_37xx_myapp.bin` (or run `sudo ./tools/run_test.sh selftest` for the full Gate B suite).
