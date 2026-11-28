KERNEL_VER ?= 6.6.163
BUILD_DIR  := $(CURDIR)/build
SRC_DIR    := $(BUILD_DIR)/linux-$(KERNEL_VER)
TARBALL    := $(BUILD_DIR)/linux-$(KERNEL_VER).tar.xz
TARBALL_URL := https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(KERNEL_VER).tar.xz

PATCHES_DIR := $(CURDIR)/patches
DEFCONFIG   := $(CURDIR)/npu3720_defconfig

ARCH       := sparc
LLVM       := 1
LLVM_FLAGS := LLVM=1 ARCH=sparc

all: build

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

fetch: $(SRC_DIR)/Makefile

$(TARBALL): | $(BUILD_DIR)
	@echo "Fetching Linux $(KERNEL_VER)..."
	curl -fL -o $(TARBALL) $(TARBALL_URL)

$(SRC_DIR)/Makefile: $(TARBALL)
	@echo "Extracting Linux $(KERNEL_VER)..."
	tar -xf $(TARBALL) -C $(BUILD_DIR)
	touch $@

patch: fetch
	@if [ -d "$(PATCHES_DIR)" ] && ls $(PATCHES_DIR)/*.patch >/dev/null 2>&1; then \
		for p in $(PATCHES_DIR)/*.patch; do \
			echo "Applying patch $$p..."; \
			patch -p1 -N -d $(SRC_DIR) -r - < $$p || true; \
		done; \
	else \
		echo "No patches found in $(PATCHES_DIR)"; \
	fi

config: patch
	@echo "Applying npu3720_defconfig..."
	cp $(DEFCONFIG) $(SRC_DIR)/arch/sparc/configs/npu3720_defconfig
	$(MAKE) -C $(SRC_DIR) $(LLVM_FLAGS) npu3720_defconfig

build: config
	@echo "Building vmlinux (sparcel, LLVM=1)..."
	$(MAKE) -C $(SRC_DIR) $(LLVM_FLAGS) -j$$(nproc) vmlinux

clean:
	@if [ -d "$(SRC_DIR)" ]; then \
		$(MAKE) -C $(SRC_DIR) $(LLVM_FLAGS) clean; \
	fi

distclean:
	rm -rf $(BUILD_DIR)

.PHONY: all fetch patch config build clean distclean
