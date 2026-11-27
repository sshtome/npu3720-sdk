#!/bin/bash
# Safe firmware test runner for Intel NPU 3720.
# Usage: sudo ./tools/run_test.sh {sched0|sched1|restore} [fw_image]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FW_DST="/lib/firmware/intel/vpu/vpu_37xx_v1.bin"
FW_ZST="/lib/firmware/intel/vpu/vpu_37xx_v1.bin.zst"
MODE="${1:-sched1}"
IMG="${2:-$SDK_DIR/examples/shell/vpu_37xx_shell.bin}"

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: run with sudo: sudo $0 $*" >&2
    exit 1
fi

if [ ! -f "$FW_ZST" ]; then
    echo "Error: golden firmware $FW_ZST not found. Aborting for safety." >&2
    exit 1
fi

case "$MODE" in
sched0|sched1)
    if [ ! -f "$IMG" ]; then
        echo "Error: firmware image $IMG not found (run 'make' first)." >&2
        exit 1
    fi
    if ! lsmod | grep -q '^npu_mmio'; then
        if [ -f "$SDK_DIR/tools/npu_mmio/npu_mmio.ko" ]; then
            insmod "$SDK_DIR/tools/npu_mmio/npu_mmio.ko" || true
        fi
    fi
    cp "$IMG" "$FW_DST"
    chmod 644 "$FW_DST"
    modprobe -r intel_vpu 2>/dev/null || true
    if [ "$MODE" = "sched0" ]; then
        modprobe intel_vpu sched_mode=0
    else
        modprobe intel_vpu
    fi
    sleep 3
    echo "== dmesg =="
    dmesg | grep -iE 'intel_vpu|ivpu|JSM' | tail -10
    echo "== fw_log =="
    cat /sys/kernel/debug/accel/0000:00:0b.0/fw_log 2>/dev/null | tail -30 || true
    ;;
restore)
    rm -f "$FW_DST"
    modprobe -r intel_vpu 2>/dev/null || true
    modprobe intel_vpu
    sleep 3
    dmesg | grep -iE 'Initialized|Failed' | tail -4
    ls -la /dev/accel/
    ;;
*)
    echo "Usage: sudo $0 {sched0|sched1|restore} [fw_image]" >&2
    exit 1
    ;;
esac
