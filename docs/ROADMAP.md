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
| Free-running timer & uptime | `GPTIMER` / `TIM` (`0x2602009C` stalls when WDOG off) | Open | **M1 (`v0.2.0`)** |
| GRLIB L2C MTRR uncached window | `0x27000080+` (`0x90000000..0x90200000`) | Partially decoded `[VERIFIED-CODE]` | **M1 (`v0.2.0`)** |
| Interrupt controller (`IRQMP`) | `0x26000230` / `0x20010230`, `tt=0x11..0x1F` | Address `[VERIFIED-CODE]` | **M1 (`v0.2.0`)** |
| Automated hardware selftest | `examples/selftest` + `tools/run_test.sh` | Planned | **M1 (`v0.2.0`)** |
| Power & clocks (`CPR` / PUnit) | `0x26010040`, tile wake/clock gates | Partial `[DERIVED]` | **M2 (`v0.3.0`)** |
| `NNCMX SRAM` | `0x2E000000` (2 × 2 MB tiles) | Base `[VERIFIED-CODE]` | **M2 (`v0.3.0`)** |
| LeonRT `SRMMU` & TBU mapping | `ASI 0x18`/`0x19` / TBU interaction | Open | **M2 (`v0.3.0`)** |
| Dual-core Leon (Core 1 / LNN) | `%asr17[31:28]=1`, `0x20010230`, mailbox | Open | **M3 (`v0.4.0`)** |
| `NNDMA` engine | DDR ↔ CMX descriptors, IRQ, throughput | Open | **M3 (`v0.4.0`)** |
| `4× SHAVE DSP` | `0x2F800000+` (`+0x1000/+0x6000/+0x7000`) | Window `[VERIFIED-CODE]` | **M4 (`v0.5.0`)** |
| `2× NCE / DPU` compute | `0x2EF30000+`, variant/invariant descriptors | Window `[VERIFIED-CODE]` | **M4 (`v0.5.0`)** |
| Full JSM & `/dev/accel/accel0` offload | `CREATE_CMD_QUEUE`, `SUBMIT_CMD`, `ENGINE_RESET` | Open | **M5 (`v1.0.0`)** |

---

## 2. Milestones & Acceptance Checkpoints

### M1 (`v0.2.0`) — LeonRT System Peripherals & Test Harness
**Goal:** Replace polling and manual cache flushes with hardware timers, `IRQMP` interrupts, `MTRR` uncached regions, and an automated on-target `selftest`.
- **M1.1 Timer:** Identify and initialize a free-running hardware timer (`GPTIMER` or `TIM` configuration) independent of disabled watchdog; calibrate `npu_ticks()` and `npu_uptime_ms()` against host wall clock.
- **M1.2 GRLIB L2C MTRR:** Decode `access_ctrl` modes and `CTRL` bits on `0x27000000`; map `0x90000000..0x90200000` as uncached or write-through without breaking code/boot; verify host visibility without explicit per-packet `clean_range`.
- **M1.3 IRQMP & Interrupt Dispatch:** Implement `IRQMP` (`0x26000230`) driver and `tt=0x11..0x1F` trap dispatcher in `crt0.S`; route H2D FIFO and timer IRQs.
- **M1.4 Hardware Selftest:** Add `examples/selftest` (register windows depth test, `fault_skip`, monotonic `uptime_ms`, non-zero IRQ counters, MTRR coherency test) and automated pass/fail verification in `tools/run_test.sh`.
- **Checkpoint `v0.2.0`:** `sched_mode=1` boots cleanly; `uptime_ms` advances monotonically; H2D and timer IRQs fire; MTRR coherency verified; `selftest` outputs `PASS`; stock `.zst` firmware restored.

### M2 (`v0.3.0`) — Fast Local Memory, Power/Clocks & MMU
**Goal:** Enable compute tile SRAM, document power/clock gating, and establish the MMU model.
- **M2.1 CPR / PUnit:** Reverse-engineer clock enables, PLL/frequency controls, and tile power gating registers; verify read-back `[VERIFIED-HW]`.
- **M2.2 NNCMX SRAM (`0x2E000000`):** Power on both 2 MB CMX tiles, implement R/W integrity tests, simple bump/pool allocator, and measure CMX vs DDR bandwidth.
- **M2.3 LeonRT SRMMU:** Inspect `SRMMU` / `TBU` page tables and document either a minimal bare-metal MMU setup or verified identity/bypass requirements.
- **Checkpoint `v0.3.0`:** Both 2 MB CMX tiles pass R/W integrity checks; `CPR` registers verified on hardware; tagged `v0.3.0`.

### M3 (`v0.4.0`) — Second Leon Core (Core 1) & NNDMA
**Goal:** Unlock symmetric/asymmetric dual-core Leon execution and hardware DMA transfers.
- **M3.1 Core 1 Boot & SMP Sync:** Decode `CPR_LEON_RT_VEC` / reset vector for Core 1 (`%asr17[31:28]=1`), configure Core 1 `IRQMP` (`0x20010230`), implement mailbox + spinlock primitives, and run a dual-core execution test.
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
