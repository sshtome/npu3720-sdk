# Operating System & RTOS Bring-Up Guide for Intel NPU 3720

This guide provides the complete specification, startup contract, hardware requirements, and driver primitives needed to port or run a third-party Operating System (e.g. RTEMS, Zephyr, FreeRTOS, or a Unix-like kernel with MMU support) on the **Intel NPU 3720** (Arrow Lake / Meteor Lake, PCI `8086:ad1d`).

All information and register addresses in this guide have been verified on live silicon (`[VERIFIED-HW]`).

---

## 1. System & Architecture Overview

The Intel NPU 3720 features two 32-bit SPARC v8 processors operating in **Little-Endian** mode (`sparcel`):

| Property | Core 0 (LeonRT) | Core 1 (LeonNN) |
|---|---|---|
| Role | System controller & host IPC | Neural compute & worker |
| `%asr17` | `0x02E00587` (Core ID = 0) | `0x22E00187` (Core ID = 2) |
| FPU | **Hardware FPU present** (`PSR.EF=1`) | **No FPU** (`-msoft-float`, `PSR.EF=0`) |
| Register Windows | `NWINDOWS = 8` (`%asr17[4:0] = 7`) | `NWINDOWS = 8` (`%asr17[4:0] = 7`) |
| L1 Cache | 16 KB I-cache, 16 KB D-cache | 16 KB I-cache, 16 KB D-cache |
| L1 D-Cache Snooping | Supported (`CCR.DS` bit 23) | Supported |
| SRMMU (ASI `0x19`) | Present, 256-context 3-level | Present, 256-context 3-level |
| Interrupt Controller | `ICB @ 0x26000000` (128 IRQs) | `ICB @ 0x2F000000` (128 IRQs) |
| Clock Frequency | ~366–384 MHz (at PUnit WP 22) | ~366–384 MHz |

---

## 2. Memory Map & Layout

```
0x00000000 ┌──────────────────────────────────────────────┐
           │ Optional Virtual User Space (SRMMU)          │
0x10000000 ├──────────────────────────────────────────────┤
           │ Virtual Alias Test Window (SRMMU -> NNCMX)   │
0x20000000 ├──────────────────────────────────────────────┤
           │ Internal MMIO Window (Uncached, 512 MB)      │
           │   0x20000000: Host Subsystem & SSID Map     │
           │   0x26000000: LeonRT Subsystem & ICB        │
           │   0x26028000: System Timers (38.4 MHz)      │
           │   0x27000000: GRLIB L2 Cache Controller     │
           │   0x2E000000: NNCMX SRAM (4 MB, 2 Tiles)    │
           │   0x2EF60000..0x2EFA0000: NNDMA Engines & Decoders │
           │   0x2F000000: LeonNN Subsystem & ICB        │
           │   0x2F008000: LeonNN Boot Control           │
0x84800000 ├──────────────────────────────────────────────┤
           │ Boot Parameters (4 KB, read-only from Host)  │
0x84801000 ├──────────────────────────────────────────────┤
           │ Firmware Version String (4 KB)               │
0x84802000 ├──────────────────────────────────────────────┤
           │ Firmware Runtime Image (64 MB DDR BO)        │
           │   0x84803000: Entry Point & Trap Table       │
           │   0x87FE0000: LeonNN Stack Top (64 KB)       │
           │   0x87FF0000: LeonRT Stack Top (64 KB)       │
0x90000000 ├──────────────────────────────────────────────┤
           │ Shared Host/Device DDR (Uncached via MTRR0)  │
           │   0x90000000: fw_log critical buffer (512 KB)│
           │   0x90180000: IPC rings (Host <-> NPU)       │
0xA0000000 └──────────────────────────────────────────────┘
```

### Critical Addresses for OS Porting
- **LeonRT Entry Point:** `0x84803000`
- **LeonRT Trap Table:** `0x84803000` (must match `%tbr`)
- **LeonNN Entry Point:** `0x84809000` (must be 4 KB aligned)
- **LeonRT Stack Top:** `0x87FF0000`
- **LeonNN Stack Top:** `0x87FE0000`
- **DMA Descriptors:** `0x2E008000` (in NNCMX Tile 0)

---

## 3. Host Boot Contract & JSM Handshake

The host kernel driver (`intel_vpu`) allocates a 64 MB Buffer Object (`fw->mem`) and loads the firmware image. To achieve a clean, supported boot under the default hardware scheduler (`sched_mode=1`):

1. **Firmware Header (`npu_fw_header`):**
   - Must set `image_load_address = 0x84802000`, `entry_point = 0x84803000`, `runtime_size = 0x04000000`.
   - Set `api_version[0] >= 3.0` (BOOT IPC supported).
   - Set `api_version[4] >= 3.24` (JSM supported; selects `sched_mode=1`).
2. **Initial IPC BOOT Handshake:**
   - Initialize IPC rings at `0x90180000`.
   - Send an IPC message on channel `0x3FF` with `data_addr = 0x424F4F54` (`"BOOT"`).
3. **Mandatory JSM Messages (Hardware Scheduler Handshake):**
   - The host sends `SET_PRIORITY_BAND_SETUP` (`0x1113`). Respond with `0x2214`.
   - The host periodically sends `QUERY_ENGINE_HB` (`0x1104`). Respond with `0x2204`.
   - Failing to respond causes a host watch-dog timeout and reset of `/dev/accel/accel0`.

---

## 4. CPU Early Initialization (`crt0.S`)

Every OS kernel must perform the following low-level sequence on LeonRT before enabling interrupts or switching contexts:

### 4.1 Watchdog & Caches
```assembly
/* 1. Force initial PSR: PIL=15, S=1, ET=0, CWP=0 */
set     0x0F80, %g1
wr      %g0, %g1, %psr
nop; nop; nop

/* 2. Disable Hardware Watchdog */
set     0x260200A4, %g1
st      %g0, [%g1]

/* 3. Flush Instruction Cache */
.word   0x81D82000
nop; nop; nop
```

### 4.2 Register Windows & Trap Table
LeonRT has 8 register windows (`NWINDOWS = 8`).
```assembly
/* 4. Set Trap Base Register (%tbr) */
set     trap_table, %g1
wr      %g1, %tbr
nop; nop; nop

/* 5. Set Window Invalid Mask: window 1 invalid for CWP=0 */
wr      %g0, 2, %wim
nop; nop; nop

/* 6. Enable Traps (ET=1) and Hardware FPU (EF=1): PSR = 0x1FA0 */
set     0x1FA0, %g1
wr      %g0, %g1, %psr
nop; nop; nop

/* 7. Initialize Floating-Point Status Register (%fsr) */
st      %g0, [%sp - 4]
ld      [%sp - 4], %fsr
nop; nop; nop
```

> [!IMPORTANT]
> **`PSR.ET = 1` Requirement:** In LeonRT, synchronous traps (`0x05` overflow, `0x06` underflow, `0x09` data fault) with `ET=0` cause a **silent processor reset**. `ET=1` must be enabled before any subroutine call.
>
> **FPU on LeonNN:** Core 1 (`LeonNN`) does **NOT** have an FPU. For Core 1, code must be compiled with `-msoft-float` and `PSR.EF` must remain `0`.

---

## 5. OS Primitives

### 5.1 System Calls & Traps (`ta 0x10`)
Software trap slot `tt = 0x90` (`ta 0x10`) is designated for system calls and cooperative thread yielding:
- The trap handler receives arguments in `%o0..%o5` (which appear as `%i0..%i5` in the trap window).
- Return values are written to `%i0` before returning via `jmp %l2; rett %l2 + 4`.
- Verified on silicon: `npu_syscall_yield()` increments `g_syscall_cnt` and returns `0x4F53` (`"OS"`).

### 5.2 Atomic Instructions (Multicore & Spinlocks)
Both cores support SPARC v8 atomic primitives on both DDR and NNCMX SRAM:
- **`swap [addr], %reg` (32-bit):** Atomically swaps a 32-bit register with memory. Verified in DDR and NNCMX SRAM.
- **`ldstub [addr], %reg` (8-bit):** Reads byte from memory, sets memory byte to `0xFF`. Perfect for TAS spinlocks.

```c
static inline u32 npu_atomic_swap(volatile u32 *addr, u32 val) {
    __asm__ volatile("swap [%1], %0" : "+r"(val) : "r"(addr) : "memory");
    return val;
}

static inline u8 npu_atomic_ldstub(volatile u8 *addr) {
    u32 val = 0;
    __asm__ volatile("ldstub [%1], %0" : "=r"(val) : "r"(addr) : "memory");
    return (u8)val;
}
```

### 5.3 Interrupt Controller (ICB) & Software IPI
- **Base Address:** `0x26000000` (LeonRT) / `0x2F000000` (LeonNN).
- **128 Interrupts** grouped into 4 banks of 32 lines.
- **Software Forcing / IPI:** Write `(1 << (irq & 31))` to `ICB_FORCE` (`base + 0x250 + 4 * (irq >> 5)`).
- **Interrupt Dispatch Trampoline:**
  - Enters with `ET=0` and arbitrary `CWP`.
  - Must spill registers to stack if the trap window matches `%wim`.
  - Set `PIL=15` and `ET=1` before invoking C interrupt service routines.
  - Read `ICB_ACK` (`0x26000230`) to obtain the active IRQ number.
  - Clear the IRQ by writing `(1 << bit)` to `ICB_CLEAR` (`0x26000220 + 4*bank`), followed by a dummy read of `0x26000000` to flush posted writes.

### 5.4 Timers & OS Ticks
- **High-Resolution Clock:** `TIM_PERF_FREE_CNT` (`0x26029000/04`) runs at **38.4 MHz**. Use atomic 64-bit `ldd` instruction. Never drifts; independent of CPU clock frequency scaling.
- **Periodic Scheduler Tick:** Timer0 (`0x2602800C..14`) configured for `IRQ 108`.

---

## 6. Memory Management: SRMMU

LeonRT contains a full SPARC Reference MMU accessible via ASI `0x19` (registers) and ASI `0x18` (TLB operations).

### 6.1 MMU Architecture
- **Context Table:** 256 entries (1024 bytes, 4096-byte aligned). Pointed to by `NPU_SRMMU_CTPR` (`ASI 0x19, offset 0x100`).
- **3-Level Page Tables:**
  - Level 1: 256 entries × 16 MB sections = 4 GB address space.
  - Level 2: 64 entries × 256 KB.
  - Level 3: 64 entries × 4 KB pages.
- **Entry Types (`ET`, bits [1:0]):**
  - `0x0`: Invalid
  - `0x1`: Page Table Descriptor (PTD)
  - `0x2`: Page Table Entry (PTE)
- **PTE Permissions & Flags:**
  - Bit 7: Cacheable (`C`)
  - Bit 6: Modified (`M`)
  - Bit 5: Referenced (`R`)
  - Bits 4:2: Access Permissions (`ACC`): `7` = Privileged RWX
  - Bit 1:0: `ET = 2`
  - Uncached Section PTE: `0x7E`
  - Cached Section PTE: `0xFE`

### 6.2 Working Identity & Virtual Translation
The SDK provides `sdk/src/mmu.c`, which builds a Level-1 16 MB identity map:
- MMIO & NNCMX (`0x20000000..0x3FFFFFFF`): uncached (`0x7E`).
- Firmware DDR (`0x84000000..0x88FFFFFF`): cached (`0xFE`).
- Shared DDR (`0x90000000..0x9FFFFFFF`): uncached (`0x7E`).
- **Virtual Translation:** `VA 0x10000000 -> PA 0x2E000000` (NNCMX Tile 0). Verified read/write through virtual address alias on live hardware.

### 6.3 TLB Flush Operations (ASI 0x18)
```assembly
sta %g0, [addr] 0x18
```
Address bits determine flush scope:
- `addr = 0x000`: Flush page (4 KB)
- `addr = 0x100`: Flush segment (256 KB)
- `addr = 0x200`: Flush region (16 MB)
- `addr = 0x300`: Flush context
- `addr = 0x400`: Flush entire TLB

---

## 7. Dual-Core SMP / AMP Boot

To run a secondary kernel or worker thread on `LeonNN Core 1`:

1. **Compile Core 1 Code:**
   - Must use `--target=sparcel-unknown-elf -msoft-float`.
   - Ensure Core 1 entry point and trap table are aligned to `0x1000` (4 KB).
2. **Launch Sequence from LeonRT (`sdk/src/leonnn.c`):**
   - Write entry address to `0x2F008000`.
   - Configure `NCE_CPR` clocks and release Tile resets.
   - Core 1 starts in CWP=0, reads its `%asr17` (`0x22E00187`), and initializes its trap table.
3. **Inter-Core Communication:**
   - Use hardware FIFO `TIM_IPC_FIFO` (`0x2F0020F0` / `0x260200F0`) with 32-bit tagged messages.
   - Use shared memory in NNCMX SRAM (`0x2E000000`) with `swap` or `ldstub` locks.

---

## 8. Verified Hardware Capabilities Matrix

| OS Feature | Mechanism | Verified Status |
|---|---|---|
| Preemptive Multitasking | Timer0 (`IRQ 108`) + ICB | `[VERIFIED-HW]` |
| Cooperative Yield / Syscall | `ta 0x10` (`tt = 0x90`) | `[VERIFIED-HW]` |
| Floating-Point Operations | Hardware FPU on LeonRT (`PSR.EF=1`, `%fsr`) | `[VERIFIED-HW]` |
| Multicore Spinlocks & Synchronization | `swap` (32-bit) & `ldstub` (8-bit) | `[VERIFIED-HW]` |
| Inter-Processor Interrupts (IPI) | `ICB_FORCE` (`0x26000250` / `0x2F000250`) | `[VERIFIED-HW]` |
| Virtual Memory & Paging | SRMMU (ASI `0x19`, Identity + Virtual Alias) | `[VERIFIED-HW]` |
| Multi-Core Execution | LeonRT + LeonNN dual-core concurrent | `[VERIFIED-HW]` |
| High-Speed Direct Memory Access | NNDMA (32 GB/s CMX crossbar, 392 MB/s DDR) | `[VERIFIED-HW]` |
