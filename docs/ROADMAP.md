# Intel NPU 3720 Bare-Metal SDK Roadmap

Target hardware: **Intel NPU 3720** (PCI `8086:ad1d`, LeonRT SPARC v8 Little-Endian).
Baseline checkpoint: **`v0.1.0`** (`a7fe0f0`) — clean-room firmware builder, `crt0` (`NWINDOWS=8`, `fault_skip`), GRLIB L2C `clean_range`, coherent `fw_log`, `BOOT` + `JSM` (`sched_mode=1`), and interactive console.

---

## 1. Hardware Coverage Matrix

| Subsystem | Address / Interface | Status | Milestone |
|---|---|---|---|
| Firmware container & boot | `npu_fw_header`, `0x84802000` / `0x84803000` | `[VERIFIED-HW]` | `v0.1.0` |
| LeonRT core & trap table | `NWINDOWS=8`, `%tbr=0x84803000`, `tt=5/6/9` | `[VERIFIED-HW]` | `v0.1.0` |
| IPC D2H / H2D & JSM boot | `0x200200F0`, `0x260200F4/FC/220`, `ASYNC_CMD=0` | `[VERIFIED-HW]` | `v0.1.0` |
| `fw_log` & Host↔NPU console | `0x90000000`, `0xCCxxxxxx` (`[21:6]`) | `[VERIFIED-HW]` | `v0.1.0` |
| L1D & GRLIB L2C cache clean | `ASI 0x11`, `0x27000004/08` | `[VERIFIED-HW]` | `v0.1.0` |
| Free-running timer & uptime | `TIM_PERF_FREE_CNT` (`0x26029000/04`, 38.4 MHz `ldd`), Timer0 (`0x2602800C..14`) | `[VERIFIED-HW]` | **M1 (`v0.2.0`)** |
| GRLIB L2C MTRR uncached window | `MTRR[0] = 0x9000FFE1` (`0x90000000..0x90200000`), `L2C_CTRL = 0x80040001` | `[VERIFIED-HW]` | **M1 (`v0.2.0`)** |
| Interrupt controller (`ICB` / LeonRT) | 128-IRQ `ICB @ 0x26000000` (`LNN @ 0x2F000000`), `tt=0x11..0x1F`, `IRQ 12/108` | `[VERIFIED-HW]` | **M1 (`v0.2.0`)** |
| Automated hardware selftest | `examples/selftest` (`5/5 PASS`) + `tools/run_test.sh selftest` | `[VERIFIED-HW]` | **M1 (`v0.2.0`)** |
| Power & clocks (`CPR` / PUnit) | `0x26010040`, AON islands 5/32/33, `0x20000160`, `0x2F008000..34` | `[VERIFIED-HW]` | **M2 (`v0.3.0`)** |
| `NNCMX SRAM` | `0x2E000000` (2 × 2 MB tiles, 262 MB/s write) | `[VERIFIED-HW]` | **M2 (`v0.3.0`)** |
| Dual-core Leon (`LeonNN Core 1`) | `%asr17[31:28]=2`, `0x2F008000`, `LNN ICB 0x2F000000`, `TIM_IPC_FIFO` tag `0xBB000000` | `[VERIFIED-HW]` | **M2 (`v0.3.0`)** |
| `NNDMA` engine | DDR ↔ CMX descriptors, IRQ, throughput | `[VERIFIED-HW]` | **M3 (`v0.4.0`)** |
| Hardware SRMMU & L1 Snooping | ASI `0x19` (Identity + Virtual Alias `VA 0x10000000 -> PA 0x2E000000`), CCR.DS snooping `0x0082400F` | `[VERIFIED-HW]` | **M3.5 (`v0.4.1`)** |
| OS Primitives & Bring-Up | `ta 0x10` syscall, `swap`/`ldstub` atomics, FPU, `ICB_FORCE`, `docs/OS_BRINGUP.md` | `[VERIFIED-HW]` | **M3.5 (`v0.4.1`)** |
| Core Clock & PLL | ~366–384 MHz (~2c/iter @ 209k ticks/1M iter), PUnit WP `0x16` (22) | `[VERIFIED-HW]` | **M3 (`v0.4.0`)** |
| `4× SHAVE DSP` | `0x2F800000+` (`+0x1000/+0x6000/+0x7000`) | Window `[VERIFIED-CODE]` | **M4 (`v0.5.0`)** |
| `2× NCE / DPU` compute | `0x2EF30000+`, variant/invariant descriptors | Window `[VERIFIED-CODE]` | **M4 (`v0.5.0`)** |
| Full JSM & `/dev/accel/accel0` offload | `CREATE_CMD_QUEUE`, `SUBMIT_CMD`, `ENGINE_RESET` | Open | **M5 (`v1.0.0`)** |

---

## 2. Milestones & Acceptance Checkpoints

### M1 (`v0.2.0`) — LeonRT System Peripherals & Test Harness ✅ `[VERIFIED-HW]`
**Goal:** Replace polling and manual cache flushes with hardware timers, `ICB` interrupts, `MTRR` uncached regions, and an automated on-target `selftest`.
- **M1.1–M1.4 Timer (`[VERIFIED-HW]`):** Enabled `TIM_PERF_FRC_EN` (`0x26028000`) and atomic 64-bit `ldd` reads of `TIM_PERF_FREE_CNT` (`0x26029000/04`, 38.4 MHz); calibrated `npu_ticks()` / `npu_uptime_ms()` against host clock without 64-bit software division.
- **M1.5 GRLIB L2C MTRR (`[VERIFIED-HW]`):** Verified `L2C_FLUSH` opcodes (`1/2/3/5/7`), enabled `MTRR[0] = 0x9000FFE1` (`0x90000000..0x90200000` uncached) + `L2C_CTRL = 0x80040001`, confirmed coherent `fw_log` and `sched_mode=1` without `l2c_clean_range()`.
- **M1.6 ICB & Interrupt Dispatch (`[VERIFIED-HW]`):** Implemented 128-IRQ `ICB` (`0x26000000`) driver and window-safe `tt=0x11..0x1F` trampoline (`crt0.S`) with `CWP`-preserving `PIL` RMW (`npu_irq_restore`); routed Timer0 Tick (`IRQ 108`, `0x2602800C..14`) and H2D FIFO (`IRQ 12`).
- **M1.7 Hardware Selftest (`[VERIFIED-HW]`):** Added `examples/selftest` (`L2C/MTRR`, register windows `26/26`, `fault_skip @ 0x40000000`, monotonic `timer`, `IRQ 12/108`) and `tools/run_test.sh selftest` (`PASS 5/5`).
- **Checkpoint `v0.2.0` (`Gate A + Gate B PASSED`):** `sched_mode=1` boots cleanly; `uptime_ms` advances monotonically; `IRQ 12` and `IRQ 108` fire without storm or reset; `MTRR` coherency verified; `SELFTEST: PASS (5/5)`; stock `.zst` firmware restored.

### M2 (`v0.3.0`) — Power/Clocks (`CPR`), `NNCMX SRAM` & `LeonNN` (`Core 1`) ✅ `[VERIFIED-HW]`
**Goal:** Enable compute tile SRAM, document power/clock gating, boot secondary LeonNN core, and establish dual-core IPC.
- **M2.1 CPR Power & Clocks (`[VERIFIED-HW]`):** Reverse-engineered full AON island power sequences (Gate Reset/Clk → Trickle → Ramp Delay → Main Power → Poll Status → Clocks → Un-isolate → Release Reset) for NCE Spine (Island 5), Tile 0 (Islands 32, 40, 41) and Tile 1 (Islands 33, 42, 43); configured `TOP_NOC` `NCE_AXI` (`0x20000160 BIT(6)`), `NCE_CPR_NOC_Q_CTRL` (`0x2F008034`), `TILE_AUX_SET` (`0x2F008024`), verified PUnit WP `0x02020016`.
- **M2.2 NNCMX SRAM 2×2 MB (`[VERIFIED-HW]`):** Powered on all 4 MB SRAM (`0x2E000000..0x2E3FFFFF`), verified bit pattern integrity, measured write bandwidth of **262 MB/s** (15× faster than DDR at 18 MB/s) and read bandwidth of **30 MB/s**.
- **M2.3 LeonNN Core 1 Bring-Up (`[VERIFIED-HW]`):** Booted secondary SPARC v8 core (`%asr17[31:28] == 2`) via `0x2F008000` with 4 KB alignment; built with `-msoft-float` (zero FPU instructions, `PSR.EF=0`), dedicated 4 KB trap table, separate stack (`0x87FE0000`), `PSR=0x0FA0` (`ET=1`); shared GRLIB L2C untouched.
- **M2.4 Dual-Core IPC & Selftest (`[VERIFIED-HW]`):** Established bidirectional messaging via `TIM_IPC_FIFO` (LRT → LNN write to `0x2F0020F0`; LNN → LRT write to `0x260200F0` with tag `0xBB000000` demuxed in `npu_jsm_poll()`); verified live CMX mailbox counter; passed `SELFTEST: PASS (7/7)` under `sched_mode=1`.
- **Checkpoint `v0.3.0` (`Gate M2-A + Gate M2-B PASSED`):** 4 MB CMX operational at 262 MB/s; LeonNN Core 2 boots and communicates over hardware FIFO; automated selftest 7/7 PASS.

### M3 (`v0.4.0`) — NNDMA Engine ✅ `[VERIFIED-HW]`
**Goal:** Unlock hardware DMA transfers between DDR and NNCMX SRAM.
- **M3.1 NNDMA Descriptors & Subsystem Setup (`[VERIFIED-HW]`):** Decoded NCE DMA Engine T0 (`0x2EF60000`) / T1 (`0x2EF90000`), Decoder T0 (`0x2EF70000`) / T1 (`0x2EFA0000`), and NOC arbitration (`0x2F004000/6000`). Configured `NCE_CPR` clocks/resets with Domain 2 mask `0xC00`. Implemented 64-byte descriptor layout in NNCMX (`0x2E008000`) with Little-Endian control word `0x01F803FC`.
- **M3.2 SMMU Host Interface SSID Translation (`[VERIFIED-HW]`):** Identified and resolved host SMMU Translation Fault (`MMU EVTQ: 0x10`) on DDR accesses: configured 32 entries of `HOST_IF_SSID_MAP` (`0x20340000`) to `0` (SSID 0 = Global Context `fw->mem`), keeping `HOST_IF_TBU_MMUSSIDV` (`0x20360004`) at `0x33`.
- **M3.3 Link Agent Kick & Coherency (`[VERIFIED-HW]`):** Integrated Link Agent 0 kick port (`engine + 0x640`) via 64-bit atomic store (`std %o4, [%0]`), completion polling via Link Agent last processed descriptor (`+0x840`), and full L1D/L2C cache coherency for DDR memory.
- **M3.4 Hardware Selftest & Bandwidth Benchmarking (`[VERIFIED-HW]`):** Added `test_dma_cmx` and `test_dma_ddr` with IEEE 802.3 CRC32 verification to `examples/selftest/main.c`. Measured **32,000 MB/s** (32 GB/s, 2 µs) on CMX0 → CMX1 crossbar (CRC `0xD5804E27`), and **392 MB/s** average round-trip on DDR ↔ CMX (CRC `0x68BD09AD`). Passed `SELFTEST: PASS (9/9)` under `sched_mode=1`.
- **Checkpoint `v0.4.0` (`Gate M3 PASSED`):** Hardware DMA transfers fully operational between DDR and NNCMX SRAM; automated selftest 9/9 PASS.

### M3.5 (`v0.4.1`) — Operating System Primitives & Hardware SRMMU ✅ `[VERIFIED-HW]`
**Goal:** Establish a complete bare-metal foundation for running 3rd-party OS / RTOS kernels with virtual memory.
- **Hardware SRMMU Driver (`sdk/src/mmu.c`, `[VERIFIED-HW]`):** Built 256-context 1-level 16 MB section translation table; verified identity mapping across MMIO, CMX, and DDR; verified virtual address translation alias (`VA 0x10020000 <-> PA 0x2E020000`).
- **OS Primitives (`[VERIFIED-HW]`):**
  - Context switch / syscall dispatcher via software trap `ta 0x10` (`tt = 0x90`, returns `0x4F53`).
  - SPARC v8 hardware atomics: 32-bit `swap` and 8-bit `ldstub` verified in both DDR and NNCMX SRAM.
  - Software interrupt / IPI via `ICB_FORCE` (`0x26000250`).
  - Hardware FPU on LeonRT (`PSR.EF = 1`, `%fsr` zeroed on boot, verified `12.5f + 27.5f = 40.0f`).
- **Hardware Selftest (`[VERIFIED-HW]`):** Added `test_os_base` verifying all 5 OS primitives in `examples/selftest/main.c`; passed `SELFTEST: PASS (10/10)`.
- **Comprehensive Documentation:** Created `docs/OS_BRINGUP.md` covering memory layouts, boot handshake, trap setup, MMU, multicore bring-up, and device drivers.
- **Checkpoint `v0.4.1`:** All OS primitives and SRMMU translation verified on silicon; tagged `v0.4.1`.

### M4 (`v0.5.0`) — Compute Engines: 4× SHAVE DSP & 2× NCE/DPU
**Goal:** Execute real compute workloads on both programmable DSPs and neural compute hardware.
- **M4.1 4× SHAVE DSP (`0x2F800000+`):** Decode SHAVE clock/reset/vector registers (`+0x1000/+0x6000/+0x7000`), load a minimal SHAVE bare-metal routine into CMX, trigger execution, and verify output written back to CMX/DDR.
- **M4.2 2× NCE / DPU (`0x2EF30000+`):** Reverse-engineer DPU invariant/variant descriptors, barrier/FIFO registers, and run a minimal hardware MAC/GEMM/convolution job verified by LeonRT.
- **Checkpoint `v0.5.0`:** Verified hardware execution on at least one SHAVE core and one NCE/DPU tile with deterministic output check; tagged `v0.5.0`.

### M5 (`v1.0.0`) — Host ↔ NPU Offload & Production Release
**Goal:** Bridge standard Linux `/dev/accel/accel0` user-space jobs directly to bare-metal NPU firmware.
- **M5.1 Extended JSM Protocol:** Implement `CREATE_CMD_QUEUE`, `SUBMIT_CMD`, `DESTROY_CMD_QUEUE`, `ENGINE_RESET`, and job completion notifications compatible with `DRM_IOCTL_IVPU_*`.
- **M5.2 High-Speed Shared Transport:** Upgrade console/control channel over shared DDR ring buffers (`npu_mmio` optional for debug only).
- **M5.3 Regression, Hardening & Docs:** Full regression run across all subsystems (`selftest`), final cleanup, and complete `SPEC_NPU3720.md` + `SDK_API.md`.
- **Checkpoint `v1.0.0`:** End-to-end user-space `/dev/accel/accel0` job submission runs on clean-room bare-metal firmware; security/cleanliness audit passes; stock `.zst` firmware restored; tagged `v1.0.0`.
