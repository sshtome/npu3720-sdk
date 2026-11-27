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
| Power & clocks (`CPR` / PUnit) | `0x26010040`, tile wake/clock gates | Partial `[DERIVED]` | **M2 (`v0.3.0`)** |
| `NNCMX SRAM` | `0x2E000000` (2 × 2 MB tiles) | Base `[VERIFIED-CODE]` | **M2 (`v0.3.0`)** |
| LeonRT `SRMMU` & TBU mapping | `ASI 0x18`/`0x19` / TBU interaction | Open | **M2 (`v0.3.0`)** |
| Dual-core Leon (Core 1 / LNN) | `%asr17[31:28]=1`, `LNN ICB 0x2F000000`, mailbox | Open | **M3 (`v0.4.0`)** |
| `NNDMA` engine | DDR ↔ CMX descriptors, IRQ, throughput | Open | **M3 (`v0.4.0`)** |
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

### M2 (`v0.3.0`) — Fast Local Memory, Power/Clocks & MMU
**Goal:** Enable compute tile SRAM, document power/clock gating, and establish the MMU model.
- **M2.1 CPR / PUnit:** Reverse-engineer clock enables, PLL/frequency controls, and tile power gating registers; verify read-back `[VERIFIED-HW]`.
- **M2.2 NNCMX SRAM (`0x2E000000`):** Power on both 2 MB CMX tiles, implement R/W integrity tests, simple bump/pool allocator, and measure CMX vs DDR bandwidth.
- **M2.3 LeonRT SRMMU:** Inspect `SRMMU` / `TBU` page tables and document either a minimal bare-metal MMU setup or verified identity/bypass requirements.
- **Checkpoint `v0.3.0`:** Both 2 MB CMX tiles pass R/W integrity checks; `CPR` registers verified on hardware; tagged `v0.3.0`.

### M3 (`v0.4.0`) — Second Leon Core (Core 1) & NNDMA
**Goal:** Unlock symmetric/asymmetric dual-core Leon execution and hardware DMA transfers.
- **M3.1 Core 1 Boot & SMP Sync:** Decode `CPR_LEON_RT_VEC` / reset vector for Core 1 (`%asr17[31:28]=1`), configure Core 1 `LNN ICB` (`0x2F000000`), implement mailbox + spinlock primitives, and run a dual-core execution test.
- **M3.2 NNDMA Driver:** Reverse-engineer descriptor format, submission queues, completion status/IRQ, and implement asynchronous `npu_dma_copy()` (DDR ↔ CMX) with CRC/integrity and throughput checks.
- **Checkpoint `v0.4.0`:** Core 0 and Core 1 exchange messages concurrently; `NNDMA` transfers DDR ↔ CMX without data corruption; tagged `v0.4.0`.

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
