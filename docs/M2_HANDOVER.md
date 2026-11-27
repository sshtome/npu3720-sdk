# M2 Static RE Handover — Power/Clocks (`CPR`), `NNCMX SRAM` & `LeonNN` (`Core 1`)

## 1. Current Project Status

- **Completed & Released:** `v0.2.0` (`af343cb` on `origin/master`, tagged `v0.2.0`) — Milestone M1 (`TIM_PERF_FREE_CNT` 38.4 MHz via `ldd`, GRLIB L2C `MTRR[0] = 0x9000FFE1`, 128-IRQ `ICB @ 0x26000000` + window-safe `tt=0x11..0x1F` trampoline, `examples/selftest` `5/5 PASS` on `sched_mode=1`).
- **Completed Static RE (M2.A / M2.B / M2.C):** All register maps, power-on sequences, CMX memory layout, `LeonNN` launch/ICB/IPC mechanisms, and ISA differences are decoded `[VERIFIED-CODE]` / `[DERIVED]` across stock firmware (`vpu_37xx_v1.bin` Stage-1 stubs + embedded ELF) and `linux-ivpu/`.
- **Scope Note:** `SRMMU` is deferred; bare-metal continues using GRLIB L2C `MTRR` (`0x27000080+`).

---

## 2. Boundary: Host (`intel_vpu`) Pre-Setup vs. LeonRT Runtime

### 2.1 Already Enabled by Host Driver Before LeonRT Starts (`[VERIFIED-CODE]`)
- **Buttress / PUnit (`ivpu_hw_btrs.c`):** exits D0i3 (`0x0060`), sends workpoint `WP_CONFIG = 0x0202` (`MTL_CONFIG_2_TILE | MTL_PLL_RATIO_4_3`), verifies `PLL_STATUS.LOCK` (`0x0040 BIT(1)`) and `VPU_STATUS.READY` (`0x0044 BIT(0)`).
- **`HOST_SS_CPR` (`0x20000084/94/98`):** un-resets and clocks `BIT(1) | BIT(10) | BIT(11)` (`TOP_NOC | DSS_MAS | MSS_MAS`).
- **AON `MSS_CPU` Island `3` (`0x20030020..2C` `BIT(3)`):** trickle, main power, un-isolate, `AON_DPU_ACTIVE` (`0x20030204 BIT(0) = 1`), `AON_VPU_IDLE_GEN` (`0x20030200 BIT(0) = 0`).
- **AXI / `TOP_NOC` (`0x20000154` `BIT(0)`, `0x20000160` `BIT(0)|BIT(1)`):** enables `TOP_SOCMMIO`, `CPU_CTRL`, `HOSTIF_L2CACHE` (leaves `BIT(6)` `NCE_AXI` **OFF**).
- **LeonRT Core 0 Launch (`0x26010040`, `0x20041040`):** `RSTRUN0` / `RESUME0` + `LOADING_ADDRESS_LO`.

### 2.2 Left OFF by Host — Must Be Enabled by Bare-Metal Firmware (`[VERIFIED-CODE]`)
- **AON Power Island `5` (`NCE` Spine + `LeonNN`):** `0x20030020..2C` `BIT(5)` (`0x20`), `AON_EN_MASK0` (`0x20030060 = 0x20`).
- **AON Power Islands `32, 40, 41` (Tile 0) & `33, 42, 43` (Tile 1):** `0x20030050..5C` (`0x00000301` and `0x00000C02`).
- **`MSSCPU_CPR` Master `NCE_MAS`:** `0x26010004/08/14/18` `BIT(9)` (`0x200`), plus domain 1 `BIT(4:2)` (`0x1C`).
- **`TOP_NOC` `NCE_AXI` Q-channel:** `0x20000160..68` `BIT(6)` (`0x40`).
- **All `NCE_CPR` (`0x2F008000..0x2F008034`)** clocks, resets, NoC Q-channels (`0x2F008034`), tile aux enables (`0x2F008024/28`), and `LeonNN` launch (`0x2F008000`).

---

## 3. CPR Register Map & Power-On Sequences (`CPU = BAR0 + 0x20000000`)

### 3.1 `HOST_SS`, `AON` & PUnit Sideband (`[VERIFIED-CODE]`)
| CPU Addr | Host BAR0 | Register | Key Bits / Notes |
|---|---|---|---|
| `0x20000118` | `0x00000118` | `HOST_SS_GEN_CTRL` | `[31:29]` platform bits (`<= 0x1FFFFFFF` enables HW ramp/poll delays; `0x60000000` bypass) |
| `0x20000160/64/68` | `0x00000160..68` | `TOP_NOC_QREQN/QACCEPTN/QDENY` | **`BIT(6)` (`0x40`) = `NCE_AXI`** (`QREQN=1`, wait `QDENY==0 && QACCEPTN==1`) |
| `0x20030020/24/28/2C` | `0x00030020..2C` | `AON_PWR_{ISO_EN,EN,TRICKLE_EN,STAT}0` | Islands `0..31`: `BIT(3)` = `MSS_CPU`, **`BIT(5)` (`0x20`) = `NCE`** (ramp `10` us) |
| `0x20030050/54/58/5C` | `0x00030050..5C` | `AON_PWR_{ISO_EN,EN,TRICKLE_EN,STAT}1` | Islands `32..63`: **`0x00000301`** (Tile 0: `32,40,41`), **`0x00000C02`** (Tile 1: `33,42,43`), ramp `7` us |
| `0x20030060/64` | `0x00030060/64` | `AON_EN_MASK0/1` | Stock `CprInit` writes `lo = 0x20`, `hi = 0` |
| `0x20032000/04` | `0x00032000/04` | `AON_PWR_LOCK0/1` | RO host lock flags |
| `0x20044000` | `0x00044000` | `DPU_CLK_MUX` `[DERIVED]` | `BIT(0)` DPU/PLL clock ratio select |
| `0x20080000..C00` | `0x00080000+` | `SB_FIFO_TX[0..3]` | PUnit sideband command FIFO (if tile WP re-request needed) |
| `0x20082020` | `0x00082020` | `WORKPOINT_CONFIG_MIRROR` | `[15:0]` active PLL ratio (`MHz = ratio * 50 * 2 / 3`), `[31:16]` active `WP_CONFIG` (`0x0002/0102/0202`) |

### 3.2 `CPU_SS_MSSCPU_CPR` (`0x26010000`, Domains `0..1`) & `NCE_CPR` (`0x2F008000`, Domains `2..4`) (`[VERIFIED-CODE]`)
| CPU Addr | Role | Bits / Devices |
|---|---|---|
| `0x26010000/04/08` | `MSSCPU_CPR_CLK_{GET,SET,CLR}` | `BIT(0)` (`dev[0]`), `BIT(1)` (`dev[1]` `CPU_DSU`), `BIT(4:2)` (`0x1C`), **`BIT(9)` (`0x200`, `NCE_MAS`)** |
| `0x26010010/14/18` | `MSSCPU_CPR_RST_{GET,SET,CLR}` | Same bit layout (`CLR` = assert reset, `SET` = de-assert `RSTN`) |
| `0x26010040` | `CPR_LEON_RT_VEC` | Host boots LeonRT (`RSTRUN0`/`RESUME0`); **not** used by FW to launch LeonNN |
| `0x2F008000` | `LEON_NN_VEC` / `CTRL` | `[31:12]` 4 KB-aligned entry; `BIT(1)` (`0x2`) = `RUN/RESUME`, `BIT(2)` (`0x4`) = `RSTRUN` pulse |
| `0x2F008008/14/18` | `NCE_CLK_{GET,SET,CLR}` | Dom 2: `BIT(1)` (`dev[4]` LNN dep), `BIT(5)` (Tile 0), `BIT(6,7)` (Tile 1), `BIT(10,11)` (NNDMA). Dom 3: `BIT(0)` (`dev[3]` LeonNN), `0x0C \| 0xF11` (`dev[2]` `NCE_SPINE`). Dom 4: `BIT(12)` (Tile 0), `BIT(13)` (Tile 1), `BIT(20..23)` (SHAVE) |
| `0x2F00800C/1C/20` | `NCE_RST_{GET,SET,CLR}` | Same bit layout (`CLR` = assert, `SET` = release) |
| `0x2F008024/28` | `TILE_AUX_{SET,CLR}` `[DERIVED]` | Open discrepancy: verify `(1 << bit) >> 12` (`0x01`/`0x02`) vs `0x301`/`0xC02` (see §6) |
| `0x2F008034` | `NCE_NOC_Q_CTRL` | 4 × 3-bit channels: `QREQN = 0x924`, `QACCEPTN = 0x492`, `QDENY = 0x249` |
| `0x2D000000`, `0x2D400010/14` | `LNN_DSU_CTRL`, `PC`/`NPC` | `BIT(6)` debug mode, `BIT(7)` ext dbg, `BIT(11)` (`0x800`) power-down (`wr %asr19`) |

### 3.3 `HglCprIslandEnable(island, dom, master_mask)` Sequence (`[VERIFIED-CODE]`)
Exact order (**Gate Reset/Clk → Trickle → Ramp Delay → Main Power → Poll Status → Clk → Un-isolate → Release Reset**):
1. `mask = island_mask(island)` (`lo = 0x20` for Island `5`; `hi = 0x301` for `32`; `hi = 0xC02` for `33`).
2. `RST_CLR(dom, master_mask)` then `CLK_CLR(dom, master_mask)`.
3. `AON_PWR_ISLAND_TRICKLE_EN |= mask` (`0x20030028` for `<32`, `0x20030058` for `>=32`).
4. Delay `10` us (Island `5`) or `7` us (Islands `32/33`) when `(HOST_SS_GEN_CTRL & 0xE0000000) == 0`.
5. `AON_PWR_ISLAND_EN |= mask` (`0x20030024` / `0x20030054`).
6. Poll `(AON_PWR_ISLAND_STATUS & mask) != 0` (`0x2003002C` / `0x2003005C`), up to 6 × 1 us.
7. `CLK_SET(dom, master_mask)`.
8. `AON_PWR_ISO_EN &= ~mask` (`0x20030020` / `0x20030050`).
9. `RST_SET(dom, master_mask)`.

### 3.4 Full Bring-Up Order for NCE Spine + Tiles (`[VERIFIED-CODE]`)
1. **NCE Spine (Island `5`):**
   - `HglCprIslandEnable(5, dom=0, 0x200)` (`0x260100xx` `BIT(9)` `NCE_MAS`) + enable `dom=0` `BIT(9)` (`0x200`).
   - Allow NoC traffic: check `0x20000160 & 0x40` (set `BIT(6)` if clear, wait `QDENY==0 && QACCEPTN==1`), set `0x2F008034 |= 0x924`, poll `(val & 0x492) == 0x492` (`QDENY` `0x249 == 0`).
   - Enable `NCE_SPINE` on `NCE_CPR` dom 3 (`0x2F008014/1C`): `0x0C`, then `0xF11`.
2. **Tile 0 (`0x2E000000`) / Tile 1 (`0x2E200000`):**
   - Tile 0: `HglCprIslandEnable(32, dom=2, 0x00301000)` (`32,40,41` → `hi=0x301`) → write `0x2F008024` → enable dom 4 `0x1000` (`BIT(12)`) & dom 2 `0x20` (`BIT(5)`).
   - Tile 1: `HglCprIslandEnable(33, dom=2, 0x00C02000)` (`33,42,43` → `hi=0xC02`) → write `0x2F008024` → enable dom 4 `0x2000` (`BIT(13)`) & dom 2 `0xC0` (`BIT(6)|BIT(7)`).

---

## 4. `NNCMX SRAM` (`0x2E000000..0x2E3FFFFF`, 4 MB)

- **Geometry (`[VERIFIED-CODE]`):**
  - Total **4 MB** (`TOTAL_USED_NN_CMX = 0x400000`), check `(addr & 0xFFC00000) == 0x2E000000`.
  - **Tile 0:** `0x2E000000..0x2E1FFFFF` (2 MB; Slice 0 `0x2E000000..0x2E0FFFFF`, Slice 1 `0x2E100000..0x2E1FFFFF`).
  - **Tile 1:** `0x2E200000..0x2E3FFFFF` (2 MB, stride `0x200000`; Slice 2 `0x2E200000..0x2E2FFFFF`, Slice 3 `0x2E300000..0x2E3FFFFF`).
  - **Broadcast / Fused Tile Alias:** `0x2EC00000..0x2EDFFFFF` (`base + 0xC00000`).
  - **Context Firewalls (disabled unless configured):** `0x2F008100..0x2F0083A0` (`0x2F008100/10` slice ctx, `0x2F008140/50/80/90` stolen window).
- **Initialization Before R/W (`[VERIFIED-CODE]`):**
  - **No HW SRAM clear, no retention-exit register, and no ECC scrub** exist in firmware power-on (`boot_params.vpu_uses_ecc_mca_signal` @ `0x478` is MCA error reporting only).
  - Once Island 5 + NoC + Tile 0/1 CPR power/clocks/resets are active, full 2×2 MB SRAM is directly R/W-accessible to LeonRT/LeonNN. (`DmaCmxWorkspaceWiper` is only an inference context-switch clear.)
- **Cache Behavior (`[VERIFIED-CODE]` / `[DERIVED]`):**
  - Stock firmware **never** configures `MTRR` for `< 0x80000000` and **never** calls L1/L2 cache flush/invalidate on CMX addresses; `0x2E000000..0x2E3FFFFF` bypasses GRLIB L2C (`0x27000000`) as internal SoC/NoC space `[DERIVED]`.
  - **Note:** `0x2F010000` is the separate **SHAVE L2C** (`[VERIFIED-CODE]`); `0x2D000000/0x2D400000` is **LeonNN DSU**.
  - On live hardware in `v0.2.0`, LeonRT `CCR` is `0x0002000F` (`DS=0` snooping bit 23 not set). Verify in Gate M2-A/B whether LeonRT L1 D-cache caches `0x2E000000` (check bandwidth & coherency with `LeonNN`).

---

## 5. `LeonNN` (`Core 1`, `%asr17[31:28] == 2`) Launch, `LNN ICB` & IPC

### 5.1 Launch Sequence (`[VERIFIED-CODE]`)
Requires Island `5` + `NCE_MAS` (`0x26010004/14 BIT(9)`) + NoC (`0x20000160 BIT(6)`, `0x2F008034`):
1. Enable & reset LeonNN bus dep (`dev[4]`, `NCE_CPR` dom 2 `BIT(1)` = `0x2` on `0x2F008014/18/1C/20`).
2. Write 4 KB-aligned entry address (`(entry & 0xFFF) == 0`) to **`0x2F008000`**.
3. Enable `NCE_CPR` dom 3: `0x0C`, then `BIT(0)` (`0x1`) on `0x2F008014/1C`.
4. Pulse start in **`0x2F008000`**: RMW `|= 0x6`, then `(val & ~0x4) | 0x2` (holds `BIT(1)` RUN=1, pulses `BIT(2)` RSTRUN 1→0). Stop with `*(0x2F008000) &= ~0x2`.

### 5.2 `LNN ICB` (`0x2F000000`) & `TIM_IPC_FIFO` (`[VERIFIED-CODE]`)
- **`LNN ICB` base = `0x2F000000`** (`0x20010000` is Host ICB, `0x26000000` is LeonRT ICB). Identical 128-IRQ layout:
  - `+0x000..+0x1FC` `ICB_CFG(irq)`, `+0x200..+0x20C` `ICB_STATUS`, `+0x220..+0x22C` `ICB_CLEAR` (W1C + read barrier `[0x2F000000]`), `+0x230` `ICB_ACK`, `+0x240..+0x24C` `ICB_ENABLE`, `+0x250..+0x25C` `ICB_FORCE`, `+0x304..+0x330` `ICB_REDIRECT`, `+0x350..+0x35C` `ICB_DISABLE`.
- **Hardware `TIM_IPC_FIFO`** (`[0]=0x20020000` Host, `[1]=0x26020000` LRT, `[2]=0x2F002000` LNN; `src_id = (%asr17 >> 28) + 5` → **LRT = `5`**, **LNN = `7`**):
  - **LRT → LNN:** write `(desc_addr & ~0x3F) | 5` to **`0x2F0020F0`** → triggers **IRQ 12 (`0x0C`)** on `LNN ICB` (`0x2F000000`); LNN reads `0x2F0020FC` (`fill[23:16]`), pops `0x2F0020F4`, clears `0x2F000220 = 0x1000` + read `[0x2F000000]`.
  - **LNN → LRT:** write `(desc_addr & ~0x3F) | 7` to **`0x260200F0`** → triggers **IRQ 12 (`0x0C`)** on `LRT ICB` (`0x26000000`).
  - **Software IRQ / Doorbell note:** Stock LNN does **not** use `0x26200000`/`0x26300000` doorbells; cross-CPU signaling also works via `ICB_FORCE` (`0x2F000250+` / `0x26000250+`) or shared uncached memory/CMX flags + L1D flush.

### 5.3 Critical Architectural Differences: `LeonRT` vs `LeonNN` (`[VERIFIED-CODE]`)
| Parameter | `LeonRT` | `LeonNN` | SDK Implication |
|---|---|---|---|
| `%asr17[31:28]` Core ID | `0` | **`2`** | `src_id = 7`; verify `%asr17[31:28] == 2` on LNN |
| `NWINDOWS` (`%asr17[4:0]+1`) | `8` | `8` | Same `window_overflow`/`underflow` handlers |
| FPU / `PSR.EF` (`BIT(12)`) | Present (`EF=1`) | **NO FPU (`EF=0`)** | **Must compile LNN code with `-msoft-float`**; never set `PSR.EF=1` on LNN |
| Entry / `%tbr` / Stack | `0x84803000` (`4 KB` aligned) | Separate 4 KB-aligned stub | LNN **must** have its own `%tbr` (4 KB aligned), `.bss`/data or shared slot, separate `_stack_top`, and **`PSR.ET=1`** (same `ET=0 -> reset on trap` rule) |
| GRLIB L2C (`0x27000000`) | Owns & configures L2C/MTRR | Shares `0x27000000` L2C | **Only LeonRT may init `0x27000000` L2C/MTRR** before releasing LNN |
| L1 `CCR` (`ASI 0x02`) | `0x0002000F` (live HW) | Write `0x0F` / `0x0001000F` | Check `CCR` snoop (`DS` bit 23); flush L1D (`ASI 0x11`) around DDR/CMX shared data if `DS=0` |

---

## 6. Open Questions / Items to Verify Before Coding

1. **Exact write value to `0x2F008024/28` (`TILE_AUX_SET/CLR`):**
   - `M2.A` disassembled `CprSysDeviceAction` as `(1 << bit) >> 12` (`0x01` for Tile 0 `bit=12`, `0x02` for Tile 1 `bit=13`), whereas `M2.B` noted `0x301`/`0xC02`. Cross-check the exact disassembly at `0x8505acb0` (`CprSysDeviceAction`) on `vpu_37xx_v1.bin` ELF before writing `cpr.c`.
2. **Host PUnit Workpoint (`WP_CONFIG = 0x0202`):**
   - Host `intel_vpu` already programs `MTL_CONFIG_2_TILE` (`0x0202`). Test on live HW whether `NCE_SPINE` + Tiles 0/1 come up cleanly via AON + `MSSCPU_CPR` + `TOP_NOC` + `NCE_CPR` **without** touching `0x20080000` PUnit Sideband FIFO first; inspect `0x20082020` in `fw_log`.
3. **Multiplexing `0x260200F0` (`LRT H2D FIFO` vs Host JSM & Console):**
   - Host writes IPC messages (`mem_tx` pointers in `0x90180000+`) and `0xCCxxxxxx` console words into `0x260200F0`, triggering `IRQ 12` on LeonRT (`0x26000000`). If `LeonNN` also pushes descriptors into `0x260200F0` (`(addr & ~0x3F) | 7`), LeonRT's `IRQ 12` handler / `npu_jsm_poll()` **must** demux popped words cleanly by `(word & 0x3F) == 7` (or address range check) so LNN messages do not collide with host JSM/console parsing (`NPU_H2D_FIFO_ACK` `0x1000` must still ack every popped item). Alternatively, use `LNN -> LRT` via `ICB_FORCE` or a dedicated shared ring in MTRR-uncached DDR (`0x90100000+`) / CMX.

---

## 7. Next Execution Plan (for Next Agent)

### Gate M2-A — `CPR` + `NNCMX SRAM` (`cpr.c` + `cmx.c`)
1. Confirm `0x8505acb0` (`0x2F008024` value) via `llvm-objdump` on the stock ELF (`0x22000` inside system `.zst` decompressed or local copy).
2. Implement `sdk/src/cpr.c` (NCE Island 5 + NoC + Spine + Tile 0/1 power/clock/reset helpers + `0x20082020` readback) and `sdk/src/cmx.c` (`0x2E000000` & `0x2E200000` 2×2 MB init, bump/pool allocator, full R/W pattern test, DDR vs CMX bandwidth helper).
3. Add CMX + CPR checks to `examples/selftest/main.c` (`sched_mode=1`), run `sudo ./tools/run_test.sh selftest`, verify `fw_log` + clean `dmesg` + automatic `.zst` restore.

### Gate M2-B — `LeonNN` (`Core 1`) Bring-Up & Dual-Core IPC (`leonnn.c`)
1. Add clean-room `LeonNN` build rules (`-msoft-float`, separate 4 KB-aligned entry/trap-table stub + separate stack in DDR, or shared-text `-msoft-float` stub with distinct stack/TBR), setting `PSR.ET=1` without touching `0x27000000` L2C/MTRR.
2. Implement `sdk/src/leonnn.c` (`0x2F008000` target addr + `|= 0x6` / `& ~0x4 | 0x2` launch, `%asr17[31:28] == 2` check, and bidirectional sync/IPC between LeonRT and LeonNN using `TIM_IPC_FIFO` demuxed `(val & 0x3F) == 7` / `0x2F0020F0` or shared uncached DDR + `ICB_FORCE`).
3. Verify dual-core ping/counter + `selftest` `PASS` on `sched_mode=1` with `.zst` auto-restored.
4. Update `docs/SPEC_NPU3720.md`, `docs/SDK_API.md`, `docs/ROADMAP.md`, run cleanliness `grep` audit, commit, push, and tag `v0.3.0`.
