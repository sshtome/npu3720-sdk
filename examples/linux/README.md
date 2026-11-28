# Linux 6.6 LTS on Intel NPU 3720 (LeonRT SPARC v8 Little-Endian)

Experimental port of Linux 6.6 LTS (`ARCH=sparc`, `CONFIG_SPARC_LEON=y`, `sparcel`) for the **Intel NPU 3720** (PCI `8086:ad1d`).

---

## 1. Goal & Architecture

- **CPU Target:** LeonRT SPARC v8 **Little-Endian** (`sparcel`), `NWINDOWS=8` (`%asr17 = 0x02E00587`).
- **Memory Map:** 64 MB DDR runtime window at `0x84802000..0x88800000`, entry point at `0x84803000`.
- **Compiler:** Clang 21.1.8 with `LLVM=1`, target `--target=sparcel-unknown-elf`, linker `ld.lld`.
- **Upstream Kernel:** Linux 6.6.163 LTS from kernel.org.

---

## 2. Directory Structure

```
examples/linux/
├── Makefile             # fetch, patch, config, build, clean targets
├── npu3720_defconfig    # Minimal unbloated kernel configuration
├── README.md            # Documentation and progress log
├── patches/             # Patches applied on top of upstream 6.6
└── build/               # Downloaded tarball and unpacked tree (ignored by Git)
```

---

## 3. Build Instructions

```bash
# Fetch and extract kernel source
make -C examples/linux fetch

# Apply npu3720 patches
make -C examples/linux patch

# Generate .config from npu3720_defconfig
make -C examples/linux config

# Build vmlinux
make -C examples/linux build
```

---

## 4. Progress Log

### Step 1.1 — Scaffold, defconfig and fetch pipeline ✅
- **Kernel Version:** Linux 6.6.163 LTS (`https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.163.tar.xz`).
- **Toolchain:** Ubuntu Clang version 21.1.8, LLD 21.1.8.
- Added `examples/linux/build/` to root `.gitignore`.
- Created automated `fetch` target in `examples/linux/Makefile`.
- Created minimal `npu3720_defconfig` (`CONFIG_SPARC_LEON=y`, `SMP=n`, `MODULES=n`, `NET=n`, `BLOCK=n`).

### Step 1.2 — Kbuild Sparcel / Clang LLVM=1 compilation ✅
- **Patch:** `patches/0001-sparc32-sparcel-clang-le.patch`
  - `scripts/Makefile.clang`: added `CLANG_TARGET_FLAGS_sparc := sparcel-unknown-elf`.
  - `arch/sparc/Makefile`: wrapped `-fcall-used-g5` and `-fcall-used-g7` in `$(call cc-option,...)` for Clang compatibility.
  - `arch/sparc/Kconfig`: added `config CPU_LITTLE_ENDIAN` and conditioned `CPU_BIG_ENDIAN` on `!CPU_LITTLE_ENDIAN`.
  - `arch/sparc/include/uapi/asm/byteorder.h`: include `<linux/byteorder/little_endian.h>` when `CPU_LITTLE_ENDIAN` is enabled or `__LITTLE_ENDIAN__` defined.
- **Compilation Verified:**
  - Full tree (417 object files, `init/`, `kernel/`, `mm/`, `fs/`, `lib/`, `arch/sparc/`) compiled cleanly with Clang 21 without errors.
  - Intermediate archive `vmlinux.a` assembled.
  - Verified `readelf -h init/main.o`: `ELF32`, `2's complement, little endian`, `Machine: Sparc`.
  - Verified relocations in `vmlinux.a`: standard SPARC v8 relocations (`R_SPARC_32`, `R_SPARC_13`, `R_SPARC_HI22`, `R_SPARC_LO10`, `R_SPARC_WDISP22`, `R_SPARC_WDISP30`).
- **Linker Diagnosis:**
  - Upstream `ld.lld` (LLVM 21) only supports `elf64_sparc` (no `elf32_sparc`).
  - GNU Binutils BFD (`sparc-linux-gnu-ld`) only implements big-endian SPARC (`elf32-sparc`), lacking `sparcel` (generic `elf32-little` rejects SPARC relocations with `EM: 2`).
  - Next step (1.3): finalize `vmlinux` linking using custom linker wrapper / linker tool.
