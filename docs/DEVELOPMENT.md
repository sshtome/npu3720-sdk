# Development & Debugging Guide

## Prerequisites

- `clang` (with `sparcel-unknown-elf` target support)
- `llvm-objdump` / `llvm-mc` (LLVM 21 recommended)
- `python3`
- Linux kernel headers (`linux-headers-$(uname -r)`) for building `tools/npu_mmio`
- `bpftrace` and `pahole` (optional, for kernel driver tracing)

## Building

```bash
# Build SDK header checks and all examples
make

# Build and load npu_mmio debugfs helper (optional, for BAR0 inspection)
make -C tools/npu_mmio
sudo insmod tools/npu_mmio/npu_mmio.ko
```

## Safe Testing & Rollback

Always verify that your distribution's stock firmware exists at `/lib/firmware/intel/vpu/vpu_37xx_v1.bin.zst` before running tests.

```bash
# Run automated Gate B selftest (sched_mode=1, ping/pong, auto-restore stock FW)
sudo ./tools/run_test.sh selftest

# Test in OS scheduler mode (sched_mode=0)
sudo ./tools/run_test.sh sched0 examples/hello/vpu_37xx_hello.bin

# Test in HW scheduler mode (sched_mode=1, default)
sudo ./tools/run_test.sh sched1 examples/shell/vpu_37xx_shell.bin

# Interactive console
sudo python3 tools/npu_console.py

# Restore stock firmware
sudo ./tools/run_test.sh restore
```

## Disassembly & Inspection

```bash
# Inspect ELF object sections and relocations
llvm-objdump -d -r examples/shell/main.o

# Disassemble raw flat binary bytes via llvm-mc
python3 -c "print(' '.join(f'0x{b:02x}' for b in open('examples/shell/fw.bin','rb').read()[0x1000:0x1040]))" \
  | llvm-mc --disassemble --triple=sparcel -mcpu=v8
```

## Kernel Driver Tracing (`bpftrace` / `pahole`)

```bash
# Inspect intel_vpu IPC structures from BTF
pahole -C ivpu_ipc_hdr /sys/kernel/btf/intel_vpu

# Trace IPC IRQ handler while intel_vpu is loaded
sudo bpftrace -e 'kprobe:ivpu_ipc_irq_handler { printf("IPC IRQ\n"); }'
```
