#!/usr/bin/env python3
"""Interactive Host <-> NPU 3720 console client.

RX (Host -> NPU): 0xCCxxxxxx words written to H2D FIFO (BAR0 + 0x060200F0) via npu_mmio.
  Bits [21:6] carry 2 ASCII bytes per word (hardware masks bits [5:0]).
TX (NPU -> Host): reads new bytes (delta) from debugfs fw_log (zero dmesg spam).
Run as root (sudo python3 tools/npu_console.py).
"""
import os
import sys
import time

POKE_PATH = "/sys/kernel/debug/npu_mmio/poke"
FW_LOG_PATH = "/sys/kernel/debug/accel/0000:00:0b.0/fw_log"
POWER_CTRL_PATH = "/sys/bus/pci/devices/0000:00:0b.0/power/control"
H2D_FIFO_OFF = 0x060200F0


def ensure_root():
    if os.geteuid() != 0:
        print("Error: run with sudo (e.g. sudo python3 tools/npu_console.py)", file=sys.stderr)
        sys.exit(1)


def send_word(w):
    with open(POKE_PATH, "w") as f:
        f.write(f"{H2D_FIFO_OFF:x} {w:x}\n")


def send_line(line):
    data = line.encode("ascii", errors="replace") + b"\n"
    for i in range(0, len(data), 2):
        b0 = data[i]
        b1 = data[i + 1] if i + 1 < len(data) else 0
        w = 0xCC000000 | (b0 << 6) | (b1 << 14)
        send_word(w)
        time.sleep(0.005)


def read_fw_log():
    try:
        with open(FW_LOG_PATH, "r", errors="replace") as f:
            txt = f.read()
    except OSError:
        return ""
    lines = []
    in_body = False
    for line in txt.splitlines(keepends=True):
        if "log start ====" in line:
            in_body = True
            lines = []
            continue
        if "log end" in line and "====" in line:
            in_body = False
            continue
        if in_body:
            lines.append(line)
    return "".join(lines).rstrip("\n") + "\n" if lines else txt


def main():
    ensure_root()
    if os.path.exists(POWER_CTRL_PATH):
        with open(POWER_CTRL_PATH, "w") as f:
            f.write("on\n")

    seen = len(read_fw_log())

    if len(sys.argv) > 1:
        cmd = " ".join(sys.argv[1:])
        send_line(cmd)
        time.sleep(0.2)
        cur = read_fw_log()
        if len(cur) > seen:
            print(cur[seen:], end="")
        elif cur:
            print(cur, end="")
        return

    print("npu_console: type 'help', 'ping', 'info', 'ticks', 'jsm', 'peek <hex>', or 'quit'")
    while True:
        try:
            cmd = input("npu> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not cmd:
            continue
        if cmd in ("quit", "exit"):
            break
        send_line(cmd)
        time.sleep(0.15)
        cur = read_fw_log()
        if len(cur) < seen:
            seen = 0
        if len(cur) > seen:
            print(cur[seen:], end="")
            seen = len(cur)


if __name__ == "__main__":
    main()
