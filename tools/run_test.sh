#!/bin/bash
# Safe firmware test runner for Intel NPU 3720.
# Usage: sudo ./tools/run_test.sh {sched0|sched1|selftest|restore} [fw_image]
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

restore_stock() {
    rm -f "$FW_DST"
    modprobe -r intel_vpu 2>/dev/null || true
    modprobe intel_vpu
    sleep 2
}

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
selftest)
    SELFTEST_IMG="${2:-$SDK_DIR/examples/selftest/vpu_37xx_selftest.bin}"
    if [ ! -f "$SELFTEST_IMG" ]; then
        echo "Error: selftest image $SELFTEST_IMG not found (run 'make' first)." >&2
        exit 1
    fi
    trap restore_stock EXIT
    cp "$SELFTEST_IMG" "$FW_DST"
    chmod 644 "$FW_DST"
    modprobe -r intel_vpu 2>/dev/null || true
    modprobe intel_vpu
    sleep 1
    echo on > /sys/bus/pci/devices/0000:00:0b.0/power/control 2>/dev/null || true
    if ! lsmod | grep -q '^npu_mmio'; then
        if [ -f "$SDK_DIR/tools/npu_mmio/npu_mmio.ko" ]; then
            insmod "$SDK_DIR/tools/npu_mmio/npu_mmio.ko" 2>/dev/null || true
        fi
    fi

    FW_LOG_PATH="/sys/kernel/debug/accel/0000:00:0b.0/fw_log"
    PASS_FOUND=0
    for _ in $(seq 1 20); do
        if grep -q "SELFTEST: PASS" "$FW_LOG_PATH" 2>/dev/null; then
            PASS_FOUND=1
            break
        fi
        sleep 0.25
    done

    PING_OK=1
    if [ -e /sys/kernel/debug/npu_mmio/poke ]; then
        PING_OK=0
        python3 -c '
import sys, time
sys.path.insert(0, "'"$SDK_DIR"'/tools")
import npu_console as c
prev = c.read_fw_log()
c.send_line("ping")
time.sleep(0.3)
cur = c.read_fw_log()
delta = cur[len(prev):]
print("== console ping delta ==")
print(delta.strip())
sys.exit(0 if "pong" in delta else 1)
' && PING_OK=1 || PING_OK=0
    fi

    echo "== dmesg =="
    dmesg | grep -iE 'intel_vpu|ivpu|Initialized' | tail -10
    echo "== fw_log =="
    cat "$FW_LOG_PATH" 2>/dev/null || true

    if ! dmesg | tail -30 | grep -q "Initialized intel_vpu"; then
        echo "FAIL: intel_vpu did not initialize cleanly" >&2
        exit 1
    fi
    if [ "$PASS_FOUND" -ne 1 ]; then
        echo "FAIL: SELFTEST: PASS not found in fw_log" >&2
        exit 1
    fi
    if [ "$PING_OK" -ne 1 ]; then
        echo "FAIL: IRQ-driven console ping failed" >&2
        exit 1
    fi
    echo "GATE B SELFTEST PASSED"
    ;;
restore)
    restore_stock
    sleep 1
    dmesg | grep -iE 'Initialized|Failed' | tail -4
    ls -la /dev/accel/
    ;;
*)
    echo "Usage: sudo $0 {sched0|sched1|selftest|restore} [fw_image]" >&2
    exit 1
    ;;
esac
