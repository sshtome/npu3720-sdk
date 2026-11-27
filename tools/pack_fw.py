#!/usr/bin/env python3
"""100% clean-room firmware container builder for Intel NPU 3720 (vpu_37xx_v1.bin).

Layout:
  [0x0000..0x0FFF] npu_fw_header (0xCC bytes + zero padding)
  [0x1000..0x1FFF] firmware version string (null-terminated + zero padding)
  [0x2000..0x2FFF] 0x1000 zero padding (image_load_address = 0x84802000)
  [0x3000..      ] flat fw.bin (entry_point = 0x84803000)
"""
import argparse
import struct

BOOT_PARAMS_ADDR = 0x84800000
FW_VERSION_ADDR = 0x84801000
IMAGE_LOAD_ADDR = 0x84802000
ENTRY_POINT = 0x84803000
RUNTIME_SIZE = 0x04000000  # 64 MB total runtime (ivpu_fw_parse subtracts 8 KB)

VPU_BOOT_API_VER_INDEX = 0
VPU_JSM_API_VER_INDEX = 4


def build_image(fw_path, out_path, version_str="npu3720-sdk 0.1.0"):
    with open(fw_path, "rb") as f:
        fw_bin = f.read()

    ver_bytes = version_str.encode("ascii") + b"\x00"
    assert len(ver_bytes) <= 0x1000, "version string exceeds 4 KB"

    hdr = bytearray(0x1000)
    pad_2000 = b"\x00" * (ENTRY_POINT - IMAGE_LOAD_ADDR)
    img_size = len(pad_2000) + len(fw_bin)

    api_version = [0] * 16
    api_version[VPU_BOOT_API_VER_INDEX] = (3 << 16) | 24
    api_version[VPU_JSM_API_VER_INDEX] = (3 << 16) | 24

    vpu_ver = b"0.1.0".ljust(32, b"\x00")[:32]

    struct.pack_into("<IIQ", hdr, 0x00, 1, 1, IMAGE_LOAD_ADDR)
    struct.pack_into("<IQ", hdr, 0x10, img_size, ENTRY_POINT)
    hdr[0x1C:0x3C] = vpu_ver
    struct.pack_into("<I", hdr, 0x3C, 0)  # compression_type = 0
    struct.pack_into("<QIQ", hdr, 0x40, FW_VERSION_ADDR, len(ver_bytes), BOOT_PARAMS_ADDR)
    struct.pack_into("<16I", hdr, 0x54, *api_version)
    struct.pack_into("<I", hdr, 0x94, RUNTIME_SIZE)

    ver_block = ver_bytes.ljust(0x1000, b"\x00")
    out = bytes(hdr) + ver_block + pad_2000 + fw_bin

    with open(out_path, "wb") as f:
        f.write(out)

    print(
        f"{out_path}: {len(out)} bytes "
        f"(hdr 0x2000 + pad {len(pad_2000):#x} + fw {len(fw_bin)}, "
        f"image_size={img_size:#x}, entry={ENTRY_POINT:#x})"
    )


def main():
    p = argparse.ArgumentParser(description="Clean-room NPU 3720 firmware packer")
    p.add_argument("fw_bin", help="Input flat binary linked at 0x84803000")
    p.add_argument("-o", "--output", default="vpu_37xx_sdk.bin", help="Output firmware image")
    p.add_argument("--version", default="npu3720-sdk 0.1.0", help="Firmware version string")
    args = p.parse_args()
    build_image(args.fw_bin, args.output, args.version)


if __name__ == "__main__":
    main()
