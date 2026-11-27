# npu_mmio

Out-of-tree Linux kernel module providing `debugfs` BAR0 `peek` and `poke` access for Intel NPU 3720 (`8086:ad1d`) while keeping the device in `D0`.

## Build & Load

```bash
make
sudo insmod npu_mmio.ko
```

## Usage

```bash
# Read 32-bit BAR0 register at offset 0x000200FC
echo 200fc | sudo tee /sys/kernel/debug/npu_mmio/peek
sudo cat /sys/kernel/debug/npu_mmio/peek

# Write 0xCC001040 to H2D FIFO (BAR0 + 0x060200F0)
echo "060200f0 cc001040" | sudo tee /sys/kernel/debug/npu_mmio/poke
```
