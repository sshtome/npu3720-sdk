KERNEL_VER ?= 6.6.163
BUILD_DIR  := $(CURDIR)/build
SRC_DIR    := $(BUILD_DIR)/linux-$(KERNEL_VER)
TARBALL    := $(BUILD_DIR)/linux-$(KERNEL_VER).tar.xz
TARBALL_URL := https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(KERNEL_VER).tar.xz

PATCHES_DIR := $(CURDIR)/patches
DEFCONFIG   := $(CURDIR)/npu3720_defconfig

BINUTILS_VER ?= 2.43.1
BINUTILS_TAR  := $(BUILD_DIR)/binutils-$(BINUTILS_VER).tar.xz
BINUTILS_URL  := https://ftp.gnu.org/gnu/binutils/binutils-$(BINUTILS_VER).tar.xz
BINUTILS_SRC  := $(BUILD_DIR)/binutils-$(BINUTILS_VER)
BINUTILS_BLD  := $(BUILD_DIR)/binutils-build
TOOLCHAIN_DIR := $(BUILD_DIR)/toolchain
SPARCEL_LD    := $(TOOLCHAIN_DIR)/bin/sparcel-linux-gnu-ld

ARCH       := sparc
LLVM       := 1
LLVM_PREFIX := /usr/lib/llvm-21/bin/
LLVM_FLAGS := LLVM=$(LLVM_PREFIX) LD=$(SPARCEL_LD) ARCH=sparc PATH=$(TOOLCHAIN_DIR)/bin:$(LLVM_PREFIX):$$PATH

VMLINUX    := $(SRC_DIR)/vmlinux
SHIM_DIR   := $(CURDIR)/shim
SHIM_BIN   := $(SHIM_DIR)/shim.bin
VMLINUX_BIN := $(BUILD_DIR)/vmlinux.bin
UNIFIED_BIN := $(BUILD_DIR)/fw_linux_unified.bin
LINUX_FW_IMG := $(BUILD_DIR)/vpu_37xx_linux.bin
PACK_FW    := $(CURDIR)/../../tools/pack_fw.py
OBJCOPY    := $(LLVM_PREFIX)llvm-objcopy

all: image

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BINUTILS_TAR): | $(BUILD_DIR)
	@echo "Fetching binutils $(BINUTILS_VER)..."
	curl -fL -o $(BINUTILS_TAR) $(BINUTILS_URL)

$(BINUTILS_SRC)/Makefile.in: $(BINUTILS_TAR)
	@echo "Extracting binutils $(BINUTILS_VER)..."
	tar -xf $(BINUTILS_TAR) -C $(BUILD_DIR)
	@if [ -f "$(PATCHES_DIR)/binutils-0001-sparcel-ld.patch" ]; then \
		echo "Applying binutils patch..."; \
		patch -p1 -N -d $(BINUTILS_SRC) -r - < $(PATCHES_DIR)/binutils-0001-sparcel-ld.patch || true; \
	fi
	touch $@

$(SPARCEL_LD): $(BINUTILS_SRC)/Makefile.in
	@echo "Building sparcel-linux-gnu-ld in $(BINUTILS_BLD)..."
	mkdir -p $(BINUTILS_BLD)
	cd $(BINUTILS_BLD) && $(BINUTILS_SRC)/configure \
		--target=sparcel-linux-gnu \
		--prefix=$(TOOLCHAIN_DIR) \
		--disable-gdb --disable-gas --disable-gprof --disable-gprofng \
		--disable-nls --disable-werror
	$(MAKE) -C $(BINUTILS_BLD) all-bfd all-ld -j$$(nproc)
	$(MAKE) -C $(BINUTILS_BLD) install-ld
	touch $@

toolchain: $(SPARCEL_LD)

fetch: $(SRC_DIR)/Makefile $(BINUTILS_TAR)

$(TARBALL): | $(BUILD_DIR)
	@echo "Fetching Linux $(KERNEL_VER)..."
	curl -fL -o $(TARBALL) $(TARBALL_URL)

$(SRC_DIR)/Makefile: $(TARBALL)
	@echo "Extracting Linux $(KERNEL_VER)..."
	tar -xf $(TARBALL) -C $(BUILD_DIR)
	touch $@

patch: fetch
	@if [ -d "$(PATCHES_DIR)" ]; then \
		for p in $(PATCHES_DIR)/0*.patch; do \
			if [ -f "$$p" ]; then \
				echo "Applying patch $$p..."; \
				patch -p1 -N -d $(SRC_DIR) -r - < $$p || true; \
			fi; \
		done; \
	fi

config: patch $(SPARCEL_LD)
	@echo "Applying npu3720_defconfig..."
	cp $(DEFCONFIG) $(SRC_DIR)/arch/sparc/configs/npu3720_defconfig
	$(MAKE) -C $(SRC_DIR) $(LLVM_FLAGS) npu3720_defconfig

build: config
	@echo "Building vmlinux (sparcel, LLVM=1, sparcel-linux-gnu-ld)..."
	$(MAKE) -C $(SRC_DIR) $(LLVM_FLAGS) -j$$(nproc) vmlinux

$(SHIM_BIN):
	@echo "Building shim bootloader..."
	$(MAKE) -C $(SHIM_DIR)

$(VMLINUX_BIN): build
	@echo "Extracting flat vmlinux.bin..."
	$(OBJCOPY) -O binary $(VMLINUX) $(VMLINUX_BIN)

image: $(SHIM_BIN) $(VMLINUX_BIN)
	@echo "Packaging unified firmware image $(LINUX_FW_IMG)..."
	python3 $(CURDIR)/package_linux.py $(SHIM_BIN) $(VMLINUX_BIN) $(UNIFIED_BIN)
	python3 $(PACK_FW) $(UNIFIED_BIN) -o $(LINUX_FW_IMG) --version "npu3720-linux 6.6.163"

clean:
	@if [ -d "$(SRC_DIR)" ]; then \
		$(MAKE) -C $(SRC_DIR) $(LLVM_FLAGS) clean; \
	fi
	$(MAKE) -C $(SHIM_DIR) clean
	rm -f $(VMLINUX_BIN) $(UNIFIED_BIN) $(LINUX_FW_IMG)

distclean:
	$(MAKE) -C $(SHIM_DIR) clean
	rm -rf $(BUILD_DIR)

.PHONY: all fetch patch config build image clean distclean toolchain
