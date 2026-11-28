# Intel NPU 3720 Bare-Metal SDK — Documentation

Welcome to the technical documentation for the **Intel NPU 3720 Bare-Metal SDK** (Arrow Lake / Meteor Lake, PCI `8086:ad1d`, dual-core Leon SPARC v8 Little-Endian).

This directory serves as the technical reference for the SDK, hardware registers, protocols, and operating system porting.

---

## 1. Documentation Index

To maintain accuracy and avoid synchronization drift, each document has a dedicated purpose as a **single source of truth**:

| Document | Purpose & Scope |
|---|---|
| [SPEC_NPU3720.md](SPEC_NPU3720.md) | **Hardware & Protocol Specification:** Canonical reference for MMIO memory maps, register offsets, bitmasks, CPU architecture, GRLIB L2C/MTRR, SRMMU, NNDMA, and verified hardware quirks. |
| [SDK_API.md](SDK_API.md) | **C API Reference:** Complete function prototypes, parameter descriptions, data structures, and usage examples for `npu3720_sdk.h`. |
| [OS_BRINGUP.md](OS_BRINGUP.md) | **OS & RTOS Porting Guide:** Practical guide for porting third-party OS kernels (RTEMS, Zephyr, FreeRTOS, Unix kernels) to NPU 3720. Covers boot contracts, trap tables, register window safety, MMU paging, and dual-core SMP/AMP. |
| [DEVELOPMENT.md](DEVELOPMENT.md) | **Developer Workflow:** Toolchain requirements (`clang --target=sparcel-unknown-elf`), build instructions, safe test runner (`tools/run_test.sh`), and debugging workflows (`bpftrace`, `llvm-objdump`). |
| [ROADMAP.md](ROADMAP.md) | **Project Roadmap & Milestone Verification:** Subsystem coverage matrix, progression across milestones (M1–M5), and verified test results on live silicon. |
| [GateA_Report.md](GateA_Report.md) | **Gate A Test Report:** Empirical hardware test log from the initial bring-up on Intel Core Ultra 9 285K. |
| [examples/linux/README.md](../examples/linux/README.md) | **Linux 6.6 LTS Port:** Porting Linux 6.6 LTS (`ARCH=sparc`, `sparcel` Little-Endian) to NPU 3720 with GNU binutils `elf32-sparcel` patch. |

---

## 2. SDK Architecture & Source Map

The following map details the role and responsibility of each source file across the repository:

```
npu3720-sdk/
├── sdk/
│   ├── include/
│   │   ├── npu3720_hw.h      # Hardware constants, MMIO base addresses, bitmasks, packed structures
│   │   └── npu3720_sdk.h     # Public C API declarations, doc-comments, inline SPARC atomics
│   └── src/
│       ├── crt0.S            # LeonRT startup (_start), NWINDOWS=8 trap handlers, syscall, IRQ trampoline
│       ├── gen_traps.py      # Generator for 256-entry SPARC v8 trap vector table (traps_table.S)
│       ├── sdk_core.c        # High-level initialization (npu_init), event polling, safe memory reads
│       ├── mmu.c             # SPARC Reference MMU (SRMMU) 1-level 16 MB section identity & virtual mapping
│       ├── dma.c             # NNDMA engine driver (Tile 0 & 1), Link Agent kick port, SMMU SSID map
│       ├── cpr.c             # Clocks, Power & Reset (AON power islands 5/32/33, NoC channels, PUnit mirror)
│       ├── cmx.c             # NNCMX SRAM (4 MB) probing, pattern verification, and memory benchmarks
│       ├── leonnn.c          # LeonNN Core 1 lifecycle management, CMX mailbox, inter-core IPC
│       ├── lnn_crt0.S        # LeonNN startup stub, 4 KB-aligned trap table, soft-float execution
│       ├── l2c.c             # GRLIB L2 Cache Controller, MTRR uncached mapping, L1 cache flush
│       ├── irq.c             # 128-line Interrupt Control Block (ICB) driver, priority routing, force IRQ
│       ├── ipc_jsm.c         # D2H/H2D ring buffers, "BOOT" handshake, JSM request responder (0x1113, 0x1104)
│       ├── timer.c           # 38.4 MHz 64-bit performance counter (atomic ldd), uptime, periodic Timer0
│       ├── log.c             # Coherent circular tracing buffer in shared DDR (fw_log @ 0x90000000)
│       └── console.c         # Bidirectional console receiver over H2D FIFO words (0xCCxxxxxx)
├── tools/
│   ├── pack_fw.py            # Clean-room firmware image builder (generates npu_fw_header + version block)
│   ├── link.py               # Clean-room ELF32 LE static linker and memory map generator
│   ├── run_test.sh           # Safe test runner with automated stock firmware backup & restore
│   ├── npu_console.py        # Host-side interactive terminal for fw_log and console communication
│   └── npu_mmio/             # Linux kernel module providing debugfs BAR0 access for diagnostics
└── examples/
    ├── hello/                # Minimal application demonstrating boot, JSM handshake, and log output
    ├── shell/                # Interactive command shell (peek, poke, ticks, info, win12, mem test)
    └── selftest/             # Automated hardware test suite (10/10 regression tests on silicon)
```

---

## 3. Subsystem Readiness Matrix

All hardware subsystems below have been implemented and verified on physical Intel NPU 3720 hardware (`[VERIFIED-HW]`):

| Subsystem | Base Address / Interface | Verified Status | Key Performance / Capability |
|---|---|---|---|
| **Firmware Boot** | `0x84802000` / `0x84803000` | `[VERIFIED-HW]` | Clean-room header, `sched_mode=1` JSM boot |
| **LeonRT Core 0** | `%asr17 = 0x02E00587` | `[VERIFIED-HW]` | `NWINDOWS=8`, `PSR.ET=1`, Hardware FPU (`PSR.EF=1`) |
| **LeonNN Core 1** | `%asr17 = 0x22E00187` | `[VERIFIED-HW]` | 4 KB aligned boot, soft-float (`-msoft-float`), dual-core |
| **Performance Timer**| `0x26029000/04` | `[VERIFIED-HW]` | 38.400 MHz free-running, atomic 64-bit `ldd`, monotonic |
| **Periodic Timer** | `0x2602800C..14` | `[VERIFIED-HW]` | Timer0 Tick generating `IRQ 108` |
| **Interrupt Controller**| `0x26000000` / `0x2F000000` | `[VERIFIED-HW]` | 128 IRQ channels, window-safe trampoline, `ICB_FORCE` |
| **GRLIB L2 Cache** | `0x27000000` | `[VERIFIED-HW]` | `MTRR[0] = 0x9000FFE1` (uncached DDR), cache line clean |
| **Coherent Tracing**| `0x90000000` (`fw_log`) | `[VERIFIED-HW]` | Non-blocking ring buffer, zero kernel log spam |
| **Interactive Console**| `0x260200F0` (`0xCCxxxxxx`) | `[VERIFIED-HW]` | Bidirectional text terminal via H2D FIFO |
| **CPR Power & Clocks**| `0x26010000` / `0x2F008000` | `[VERIFIED-HW]` | AON Islands 5 (Spine), 32 (Tile 0), 33 (Tile 1), PUnit WP 22 |
| **NNCMX SRAM** | `0x2E000000` (4 MB) | `[VERIFIED-HW]` | 262 MB/s sequential write, full bit-pattern integrity |
| **NNDMA Engine** | `0x2EF60000` / `0x2EF90000` | `[VERIFIED-HW]` | 32 GB/s CMX crossbar, 392 MB/s DDR ↔ CMX, SMMU SSID=0 |
| **Hardware SRMMU** | SPARC ASI `0x19` | `[VERIFIED-HW]` | Identity mapping + virtual translation (`VA 0x10000000`) |
| **SPARC Atomics** | `swap` & `ldstub` | `[VERIFIED-HW]` | Hardware atomic read-modify-write on DDR and NNCMX SRAM |
| **Software Syscall** | `ta 0x10` (`tt = 0x90`) | `[VERIFIED-HW]` | Cooperative context switch / syscall trap dispatcher |
