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
