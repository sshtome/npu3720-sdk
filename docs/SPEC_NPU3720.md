# Intel NPU 3720 Hardware & Protocol Specification

Verification tags used in this document:
- `[VERIFIED-HW]` — confirmed on live silicon (`8086:ad1d`, Core Ultra 9 285K).
- `[VERIFIED-CODE]` — cross-verified against stock firmware disassembly and `linux-ivpu`.
- `[DERIVED]` — derived from the `+0x20000000` MMIO window rule or a single source.

---

## 1. LeonRT Core Architecture

| Parameter | Value | Status |
|---|---|---|
| ISA | SPARC v8, **Little-Endian** (`sparcel`) | `[VERIFIED-HW]` |
| Core | LEON3-class (Movidius LeonRT) | `[VERIFIED-CODE]` |
| `%asr17` | `0x02E00587` (`NWINDOWS - 1 = asr17[4:0] = 7` → **NWINDOWS = 8**) | `[VERIFIED-HW]` |
| Entry point | `0x84803000` (driver writes `(entry >> 9) \| 1` to `LOADING_ADDRESS_LO`) | `[VERIFIED-HW]` |
| Default `%tbr` | `0x84803000` (256 × 16-byte entries; `tt=0` is reset/entry slot) | `[VERIFIED-HW]` |
| I-cache flush | `.word 0x81D82000` (`flush 0x0`) | `[VERIFIED-HW]` |
| L1 D-cache flush | `sta %g0, [%g0] 0x11` (ASI `0x11`; ASI `0x0F` does not flush L2) | `[VERIFIED-HW]` |
| Signature check | None | `[VERIFIED-HW]` |

### Known LeonRT Quirks (`[VERIFIED-HW]`)
- `PSR.ET` must be set to `1` in `crt0.S` before any deep calls or traps: with `ET=0`, `trap 0x05/0x06/0x09` trigger a hardware reset rather than invoking the trap table.
- Async traps (`tt=0x11..0x1F`) enter with `ET=0` and arbitrary `CWP`: the trampoline must spill if `WIM` bit matches the trap window, raise `PIL=15` and enable `ET=1` before C dispatch, and restore `PSR` without clobbering the current `CWP`.
- Critical-section helpers (`npu_irq_restore`) must RMW only `PIL` bits `[11:8]` in `%psr`; writing a saved `CWP` across register-window transitions corrupts the return window.
- Writing `0xFFFFFFFF` to `%wim` is ignored; reading `%wim` 3 `nop`s after such a write hangs the core.
- `clang --target=sparcel-unknown-elf` silently truncates `ta 0x80` to `ta 0` (7-bit trap immediate).
- Userspace `mmap` of PCI BAR0 returns `0xFFFFFFFF`; host MMIO access requires `ioremap` in kernel space (e.g. `tools/npu_mmio`).

---

## 2. LeonRT Memory Map

| Range | Purpose | Status |
|---|---|---|
| `0x40000000` | Unmapped AHB hole (`< 0x80000000`): safe read triggers local `trap 0x09` without host MMU fault | `[VERIFIED-HW]` |
| `0x84800000` (4 KB) | `boot_params` | `[VERIFIED-HW]` |
| `0x84801000` (4 KB) | Firmware version string | `[VERIFIED-HW]` |
| `0x84802000..0x88800000` | Firmware runtime BO (`runtime_size = 0x04000000`, 64 MB; `ivpu_fw_parse` subtracts 8 KB for `0x84800000..0x84801FFF`) | `[VERIFIED-HW]` |
| `0x90000000..0xA0000000` | Shared DDR region (256 MB): `log_crit` `0x90000000` (512 KB), `log_verb` `0x90080000` (1 MB) | `[VERIFIED-HW]` |
| `0x90180000` (16 KB) | `mem_tx` (Host → Device IPC buffers) | `[VERIFIED-HW]` |
| `0x90184000` (8 KB) | `mem_rx` IPC header area (`64B` slot stride) | `[VERIFIED-HW]` |
| `0x90186000` (8 KB) | `mem_rx` IPC payload area (`128B` JSM slot stride) | `[VERIFIED-HW]` |
| `0xA0000000` | `global_aliased_pio` — triggers host TBU translation fault and `intel_vpu` MMU event recovery | `[VERIFIED-HW]` |
| `0x20000000..0x3FFFFFFF` | Internal MMIO window: `CPU_addr = host_BAR0 + 0x20000000` | `[VERIFIED-HW]` |
| `0x2E000000` | NNCMX SRAM (2 MB/tile, stride `0x200000`) | `[VERIFIED-CODE]` |
| `0x2EF30000` | DPU / NCE register window | `[VERIFIED-CODE]` |
| `0x2F800000` | SHAVE register windows (`+core * 0x100000`, `+0x1000/+0x6000/+0x7000`) | `[VERIFIED-CODE]` |

---

## 3. MMIO Registers

### 3.1 HOST_SS (`0x20000000` = BAR0 + `0x00000000`)

| CPU Addr | Host BAR0 | Register | Status |
|---|---|---|---|
| `0x20000108` | `0x00000108` | `HW_VERSION` | `[DERIVED]` |
| `0x20041040` | `0x00041040` | `LOADING_ADDRESS_LO` (`(entry >> 9) \| 1`) | `[DERIVED]` |
| `0x200200F0` | `0x000200F0` | `D2H_FIFO_WRITE` (`(hdr_addr & ~0x3F) \| src_id`, `src_id = 5 + core_id`) | `[VERIFIED-HW]` |
| `0x200200F4` | `0x000200F4` | `D2H_FIFO_ATM` | `[DERIVED]` |
| `0x200200FC` | `0x000200FC` | `D2H_FIFO_STAT` (`rptr[7:0]`, `wptr[15:8]`, `fill[23:16]`) | `[VERIFIED-CODE]` |
| `0x20020100` | `0x00020100` | `D2H_FIFO_DOORBELL` (`1 << src_id`, optional) | `[VERIFIED-CODE]` |

### 3.2 CPU_SS (`0x26000000` = BAR0 + `0x06000000`)

| CPU Addr | Host BAR0 | Register | Status |
|---|---|---|---|
| `0x26000000 + 4*irq` | `0x06000000+` | `ICB_CFG(irq)` (128 IRQs; e.g. `0x1E` edge / `0x1C` level) | `[VERIFIED-HW]` |
| `0x26000200 + 4*bank` | `0x06000200+` | `ICB_STATUS[0..3]` (raw/pending bits across 4 × 32-bit banks) | `[VERIFIED-HW]` |
| `0x26000220 + 4*bank` | `0x06000220+` | `ICB_CLEAR[0..3]` (W1C acknowledge + posted-write readback `[0x26000000]`) | `[VERIFIED-HW]` |
| `0x26000230` | `0x06000230` | `ICB_ACK` (read winning IRQ index; `>= 128` = spurious) | `[VERIFIED-HW]` |
| `0x26000240 + 4*bank` | `0x06000240+` | `ICB_ENABLE[0..3]` / `+0x350` `ICB_DISABLE[0..3]` (write mask bit) | `[VERIFIED-HW]` |
| `0x26000250 + 4*bank` | `0x06000250+` | `ICB_FORCE[0..3]` (software force IRQ) | `[VERIFIED-CODE]` |
| `0x26010040` | `0x06010040` | `CPR_LEON_RT_VEC` | `[DERIVED]` |
| `0x2602009C` | `0x0602009C` | `TIM_WATCHDOG` (stalls when `TIM_WDOG_EN=0`) | `[VERIFIED-HW]` |
| `0x260200A4` | `0x060200A4` | `TIM_WDOG_EN` (write `0` to disable watchdog) | `[VERIFIED-HW]` |
| `0x260200A8` | `0x060200A8` | `TIM_SAFE` (`0xF1D0DEAD` unlock key) | `[VERIFIED-CODE]` |
| `0x26021000..08` | `0x06021000..08` | `TIM_PRESCALER` / `PRESCALER_RELOAD` / `GEN_CONFIG` | `[VERIFIED-CODE]` |
| `0x26028000` | `0x06028000` | `TIM_PERF_FRC_EN` (write `1` to enable 64-bit free-running counter) | `[VERIFIED-HW]` |
| `0x2602800C..14` | `0x0602800C..14` | Timer0 Tick: `CTRL` (`0x3`), `STAT` (RMW `\|= 1` ack), `RELOAD` (`IRQ 108`) | `[VERIFIED-HW]` |
| `0x26029000..04` | `0x06029000..04` | `TIM_PERF_FREE_CNT_LO/HI` (38.4 MHz 64-bit counter, atomic read via `ldd`) | `[VERIFIED-HW]` |
| `0x260200F0` | `0x060200F0` | `H2D_FIFO_WRITE` (host pushes; lower 6 bits `[5:0]` masked by HW; `IRQ 12`) | `[VERIFIED-HW]` |
| `0x260200F4` | `0x060200F4` | `H2D_FIFO_ATM` (pop address) | `[VERIFIED-HW]` |
| `0x260200FC` | `0x060200FC` | `H2D_FIFO_STAT` (`fill[23:16]`) | `[VERIFIED-HW]` |
| `0x26020220` | `0x06020220` | `H2D_FIFO_ACK` (write `0x1000` after pop) | `[VERIFIED-HW]` |

Note: LeonNN (`LNN`) has a matching ICB window at `0x2F000000` (`[VERIFIED-CODE]`). Known LeonRT IRQs: `12` = H2D IPC FIFO (`[VERIFIED-HW]`), `106` = Doorbell (`[VERIFIED-CODE]`), `108` = Timer0 Tick (`[VERIFIED-HW]`).

### 3.3 GRLIB L2C (`0x27000000`) `[VERIFIED-HW]`

| CPU Addr | Register | Notes |
|---|---|---|
| `0x27000000` | `L2C_CTRL` | Read/modify/write `\|= 0x80000001` (`0x00040000` → `0x80040001` observed) |
| `0x27000004` | `L2C_STAT` | `0x030C2103` observed (`bit24`: 64B line vs 32B, `[21:16]`: MTRR count) |
| `0x27000008` | `L2C_FLUSH` | `(line_addr & ~(line-1)) \| op`: `1`=inv line, `2`=clean/wb line, `3`=flush line, `5`=inv all, `7`=flush all |
| `0x27000080 + 4*i` | `L2C_MTRR[i]` | `[31:18]` base, `[17:16]` AC, `[15:2]` mask, `bit1` WP, `bit0` EN (`0x9000FFE1` → `0x90000000..0x90200000` uncached) |

Note: `0x2D000000` / `0x2D400010..14` belong to a separate subsystem block, not LeonRT's GRLIB L2C.

---

## 4. Protocols

### 4.1 Firmware Container (`vpu_37xx_v1.bin`)
- `[0x0000..0x00CC]`: `npu_fw_header` (`header_version=1`, `image_format=1`, `image_load_address=0x84802000`, `entry_point=0x84803000`, `runtime_size=0x04000000`, `api_version[0] >= 3.0` (BOOT), `api_version[4] >= 3.24` (JSM, selects HW `sched_mode=1`)).
- `[0x1000..0x1FFF]`: null-terminated version string.
- `[0x2000..0x2FFF]`: `0x1000` zero padding (so `0x84803000` aligns with `entry_point` and default `%tbr`).
- `[0x3000..]`: flat `fw.bin` linked at `0x84803000`.

### 4.2 IPC & JSM
- `npu_ipc_hdr` (13 bytes packed, 64-byte slot): `data_addr`, `data_size`, `channel`, `src_node`, `dst_node`, `status` (`1=ALLOCATED`).
- **BOOT:** `data_addr = 0x424F4F54` (`"BOOT"`), `data_size = 0`, `channel = 0x3FF`.
- **JSM (`sched_mode=1`):** `VPU_IPC_CHAN_ASYNC_CMD = 0` (`[VERIFIED-HW]`).
  - `SET_PRIORITY_BAND_SETUP` (`0x1113`) → respond with `SET_PRIORITY_BAND_SETUP_RSP` (`0x2214`), `result = 0`, echoed `request_id` and `payload`.
  - `QUERY_ENGINE_HB` (`0x1104`) → respond with `QUERY_ENGINE_HB_DONE` (`0x2204`).
- **Coherency:** With `L2C_MTRR[0] = 0x9000FFE1` and `L2C_CTRL` bit 31 enabled (`[VERIFIED-HW]`), writes to `0x90000000..0x90200000` (`fw_log` + IPC buffers) bypass L2C caching without explicit `npu_l2c_clean_range()`, requiring only `npu_l1_dcache_flush()` (`ASI 0x11`) when L1D is write-back/cached. Without MTRR, both L1D and L2C must be cleaned before pushing to `D2H_FIFO_WRITE` or updating `fw_log->write_index`.

### 4.3 `fw_log` & Console
- `npu_tracing_buffer_header` (`0x90000000`, 128 bytes): `vpu_canary_start @ 0x40 = 0xCAFECAFE`, `write_index @ 0x44`, `size @ 0x50`, `header_version @ 0x54 = 1`, `header_size @ 0x56 = 128`, `format @ 0x58 = 0`, `name[16] @ 0x60`.
- **Console RX (Host → NPU):** `0xCC000000 | (b0 << 6) | (b1 << 14)` written to `H2D_FIFO_WRITE` (`0x060200F0`). Hardware masks `[5:0]`, leaving 16 bits `[21:6]` for 2 ASCII bytes per word.
- **Console TX (NPU → Host):** written directly to `fw_log` with L1/L2 flush (no `dmesg` spam).
