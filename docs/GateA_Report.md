# Gate A Verification Report (M1.4 + M1.5 + Trap/Window Fix)

**Date:** 2026-11-27  
**Target:** Intel NPU 3720 (`0000:00:0b.0`, PCI `8086:ad1d`, LeonRT SPARC v8 LE)  
**Host Driver:** `intel_vpu` (`sched_mode=1` — default HW scheduler mode)  
**Firmware Image:** `examples/shell/vpu_37xx_shell.bin` (`26919 B`, entry `0x84803000`, load `0x84802000`)

---

## 1. Root Cause & Fixes Verified

1. **`PSR.ET = 0` on BootROM entry (`sdk/src/crt0.S`):**
   - BootROM transfers control to `_start` with `PSR.ET == 0` (traps disabled). Any first synchronous trap (`tt=5` `window_overflow`, `tt=9` `fault_skip`, etc.) immediately put the LeonRT core into SPARC Error Mode without entering `trap_table`.
   - Fix (`[VERIFIED-HW]`): `_start` now initializes `PSR = 0x0F80` (`PIL=15`, `S=1`, `ET=0`, `CWP=0`) before stack/BSS/TBR/WIM setup, then enables synchronous traps via `PSR = 0x0FA0` (`PIL=15`, `S=1`, `ET=1`, `CWP=0`, external IRQs remain masked until M1.6 ICB).
2. **Trap slot register preservation (`sdk/src/gen_traps.py`):**
   - Replaced `%g1` trampoline scratch with `%l0` of the trap window (`sethi %hi(h), %l0; jmp %l0 + %lo(h)`), preserving `%g1..%g7` across all traps.
3. **Register window handlers (`sdk/src/crt0.S`, `NWINDOWS=8`):**
   - `window_overflow` (`tt=5`) and `window_underflow` (`tt=6`): save/restore `%g1` in `%l5`, clear `%wim = 0` before `save` / `restore; restore`, write rotated 8-bit `%wim` (`& 0xFF`) with 3-cycle `nop` hazards, and restore cleanly via `jmp %l1; rett %l2`.
4. **Safe MMIO reads in shell `info` (`examples/shell/main.c`):**
   - Guarded `L2C_CTRL`, `L2C_STAT`, and `MTRR(0)` reads via `npu_safe_read32()`, plus added `win12` console command (`vol_deep(12)` = `910`).

---

## 2. Hardware Test Output (`sched_mode=1`, `[VERIFIED-HW]`)

### Boot & `dmesg`
```text
[drm] Initialized intel_vpu 1.0.0 for 0000:00:0b.0 on minor 0
```

### Initial `fw_log`
```text
l2c: init_ctrl=0x00040000 init_stat=0x030C2103 ctrl=0x80040001 mtrr0=0x9000FFE1 mtrr=1 init_ccr=0x00020000 ccr=0x0002400F
boot_ticks: t0=0x00000000:000000A9 t1=0x00000000:00055461 up=9 ms
npu3720-sdk shell ready (type 'help')
jsm:chan=00000000 type=00001113 rid=00000001
```

### Interactive Console Sequence (`tools/npu_console.py`, `power/control=on`)
```text
>>> ping
pong
>>> info
NPU3720-SDK v0.1.0 NW=8 freq=38400000 Hz L2C_CTRL=0x80040001 L2C_STAT=0x030C2103 MTRR0=0x9000FFE1 mtrr=1 l1flush=1 ccr=0x0002400F
>>> ticks
lo=0x04891829 hi=0x00000000 uptime=1981 ms
>>> ticks
lo=0x078CB9AF hi=0x00000000 uptime=3298 ms
>>> peek 0x84800000
[0x84800000] = 0x00010000
>>> win12
win12=OK
>>> ping
pong
```

---

## 3. Subsystem Verification Summary

| Check | Result | Evidence |
|---|---|---|
| `sched_mode=1` JSM boot | PASS `[VERIFIED-HW]` | `SET_PRIORITY_BAND_SETUP` (`0x1113`) answered, `intel_vpu` initialized |
| `TIM_PERF_FREE_CNT` (`0x26029000/04`) | PASS `[VERIFIED-HW]` | `dt_host=1.3170s`, `dt_uptime=1317 ms`, `d_ticks=50569606` → **`38.397 MHz`** (expected `38.400 MHz`) |
| `GRLIB L2C` + `MTRR0` (`0x27000000`) | PASS `[VERIFIED-HW]` | `L2C_CTRL=0x80040001`, `L2C_STAT=0x030C2103`, `MTRR0=0x9000FFE1` (`mtrr=1`, `0x90000000..0x90200000` uncached) |
| `L1 CCR` (`ASI 0x02`) | PASS `[VERIFIED-HW]` | `init_ccr=0x00020000` → `ccr=0x0002400F` (`DCS=3`, `ICS=3`, `IB=1`) |
| `window_overflow` / `window_underflow` | PASS `[VERIFIED-HW]` | `win12=OK` (`vol_deep(12) == 910`), `info` completes, subsequent `ping` → `pong` |
| `boot_params.magic` read (`peek`) | PASS `[VERIFIED-HW]` | `[0x84800000] = 0x00010000` |
| Stock firmware restore | PASS `[VERIFIED-HW]` | `./tools/run_test.sh restore` → `[drm] Initialized intel_vpu 1.0.0`, `/dev/accel/accel0` active |
