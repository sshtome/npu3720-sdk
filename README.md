# Intel NPU 3720 Bare-Metal SDK

Clean-room bare-metal C runtime and firmware SDK for **Intel NPU 3720** (Arrow Lake / Meteor Lake, PCI `8086:ad1d`, **LeonRT SPARC v8 Little-Endian**), compatible with the stock Linux `intel_vpu` kernel driver in both `sched_mode=0` (OS) and `sched_mode=1` (HW).

## Features

- **100% clean-room image builder** (`tools/pack_fw.py`) — generates the `0x2000`-byte firmware header and version block from scratch with zero proprietary bytes.
- **Standalone `sparcel` toolchain** — `clang --target=sparcel-unknown-elf` + custom ELF32 LE static linker (`tools/link.py`).
- **SPARC v8 LE runtime (`crt0.S`)** — watchdog disable, I-cache flush (`0x81D82000`), `.bss` zeroing, canonical `NWINDOWS=8` overflow/underflow trap handlers (`%tbr = 0x84803000`), `trap 0x09` `fault_skip`, and crash dump to `fw_log`.
- **Coherent `fw_log` & GRLIB L2C (`0x27000000`)** — L1 D-cache flush (`ASI 0x11`) + L2C line clean (`0x27000008`), plus MTRR helpers.
- **Hardware SRMMU Driver** — 1-level 16 MB section identity mapping and virtual address translation (`VA 0x10000000 -> PA 0x2E000000`) via ASI `0x19`.
- **Operating System Primitives** — `ta 0x10` syscall trap dispatcher, SPARC v8 hardware atomics (`swap`, `ldstub`), LeonRT hardware FPU, and software interrupt forcing (`ICB_FORCE`).
- **NNDMA Hardware Driver** — asynchronous 1D transfers at 32 GB/s (CMX crossbar) and 392 MB/s (DDR ↔ CMX) with SMMU SSID translation.
- **Dual-Core Execution** — boots secondary `LeonNN Core 1` (`%asr17 = 0x22E00187`) with bidirectional hardware FIFO messaging (`TIM_IPC_FIFO`).
- **IPC & JSM responder** — D2H/H2D ring slots, `BOOT` handshake, `VPU_IPC_CHAN_ASYNC_CMD = 0`, `SET_PRIORITY_BAND_SETUP` (`0x1113` → `0x2214`), `QUERY_ENGINE_HB` (`0x1104` → `0x2204`), `pop-ack = 0x1000`.
- **Bidirectional console** — Host → NPU via `0xCCxxxxxx` H2D words (`2 ASCII bytes/word` in bits `[21:6]`), NPU → Host via `fw_log` delta (zero `dmesg` spam).

## Hardware Target

| Component | Details | Status |
|---|---|---|
| Device | Intel NPU 3720 (PCI `8086:ad1d`) | `[VERIFIED-HW]` |
| CPU | LeonRT, SPARC v8 **Little-Endian**, `NWINDOWS=8` (`%asr17 = 0x02E00587`) | `[VERIFIED-HW]` |
| Signature check | None (firmware image is not cryptographically verified) | `[VERIFIED-HW]` |
| Stock driver | Linux `intel_vpu` (`sched_mode=1` HW scheduler) | `[VERIFIED-HW]` |

## Quickstart

> **WARNING:** Before testing custom firmware, always back up your distribution's stock firmware (`/lib/firmware/intel/vpu/vpu_37xx_v1.bin.zst`) and ensure you can restore it with `sudo ./tools/run_test.sh restore`.

```bash
# 1. Build SDK headers and examples (hello, shell)
make

# 2. Build the debugfs BAR0 helper module (optional, required for npu_console.py)
make -C tools/npu_mmio

# 3. Boot examples/shell in stock sched_mode=1
sudo ./tools/run_test.sh sched1 examples/shell/vpu_37xx_shell.bin

# 4. Connect interactive console
sudo python3 tools/npu_console.py

# 5. Restore stock firmware when finished
sudo ./tools/run_test.sh restore
```

## Documentation

- [docs/README.md](docs/README.md) — Documentation index, SDK architecture map, and subsystem readiness summary.
- [docs/SPEC_NPU3720.md](docs/SPEC_NPU3720.md) — Hardware memory map, MMIO registers, protocols, L2C/MTRR, and LeonRT quirks.
- [docs/SDK_API.md](docs/SDK_API.md) — C API reference (`npu3720_sdk.h`), trap model, and writing custom applications.
- [docs/OS_BRINGUP.md](docs/OS_BRINGUP.md) — Comprehensive guide for porting and running 3rd-party OS / RTOS kernels (RTEMS, Zephyr, FreeRTOS).
- [docs/ROADMAP.md](docs/ROADMAP.md) — Project milestones, verification status matrix, and progress.
- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) — Toolchain setup, safe testing, `llvm-objdump`, `pahole`, and `bpftrace` workflows.

## License

MIT — see [LICENSE](LICENSE).
