#!/usr/bin/env python3
import sys

def pack_unified(shim_path, vmlinux_path, unified_out):
    with open(shim_path, 'rb') as f:
        shim = f.read()
    with open(vmlinux_path, 'rb') as f:
        vml = f.read()

    # Shim starts at 0x84803000.
    # Linux kernel is linked at VMA 0xf0004000 (PA 0x85004000).
    # Since vmlinux.bin extracted from vmlinux starts at trapbase (_start = 0xf0004000),
    # it must be placed at PA 0x85004000, which is offset (0x85004000 - 0x84803000) from shim entry.
    target_offset = 0x85004000 - 0x84803000
    pad_len = target_offset - len(shim)
    assert pad_len >= 0, f"shim binary ({len(shim)} bytes) exceeds target offset ({target_offset} bytes)"

    unified = shim + (b'\x00' * pad_len) + vml
    with open(unified_out, 'wb') as f:
        f.write(unified)

    print(f"Unified binary: {len(unified)} bytes (shim={len(shim)}, pad={pad_len}, vmlinux={len(vml)} @ 0x85004000)")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: package_linux.py <shim.bin> <vmlinux.bin> <output.bin>")
        sys.exit(1)
    pack_unified(sys.argv[1], sys.argv[2], sys.argv[3])
