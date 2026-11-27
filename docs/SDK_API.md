# NPU 3720 SDK C API Reference

Include `npu3720_sdk.h` (which includes `npu3720_hw.h`) and implement `void kmain(void)`.

## Lifecycle

```c
void npu_init(const char *fw_name);
void npu_poll(void);
```

- `npu_init(fw_name)` initializes `fw_log` (`0x90000000`), IPC/JSM ring buffers, the timer (`0x2602009C`), and sends the `BOOT` IPC message.
- `npu_poll()` polls `NPU_H2D_FIFO_STAT`, dispatches `0xCCxxxxxx` console words to `npu_console_rx_word()`, and responds to JSM messages (`0x1113`, `0x1104`).

## Cache & GRLIB L2C

```c
void npu_l1_dcache_flush(void);
void npu_l2c_clean_range(u32 addr, u32 size);
void npu_l2c_flush_all(void);
void npu_l2c_set_mtrr(u32 idx, u32 addr, u32 mask, u32 access_ctrl, int wp, int enable);
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

`npu_log_puts()`, `npu_log_hex()`, and `npu_log_dec()` automatically flush L1D + L2C so output is immediately visible in `/sys/kernel/debug/accel/0000:00:0b.0/fw_log`.

## IPC & JSM

```c
void npu_ipc_send(u32 data_addr, u32 data_size, u16 channel);
void npu_ipc_send_boot(void);
void npu_jsm_poll(void);
u32  npu_jsm_count(void);
u32  npu_jsm_last_type(void);
```

## Timer & Safe Memory Read

```c
u32 npu_ticks(void);
u32 npu_timer_freq_hz(void);
u32 npu_uptime_ms(void);
int npu_safe_read32(u32 addr, u32 *out);
```

`npu_safe_read32()` clears `g_fault_skipped`, reads `*(volatile u32 *)addr`, and returns `-1` if `trap 0x09` (`fault_skip`) fired, or `0` on success.

## Console

```c
typedef void (*npu_console_cb_t)(const char *line);
void npu_console_register(npu_console_cb_t cb);
```

Register a line callback before entering the `npu_poll()` loop. See `examples/shell/main.c` for a complete interactive shell.

## Writing a New Application

1. Copy `examples/hello/` to `examples/myapp/`.
2. Update `vpu_37xx_hello.bin` target name in `Makefile` if desired.
3. Add `examples/myapp` to `EXAMPLES` in the root `Makefile`.
4. Build with `make` and test with `sudo ./tools/run_test.sh sched1 examples/myapp/vpu_37xx_myapp.bin`.
