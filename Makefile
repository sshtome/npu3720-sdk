CC      := clang
TARGET  := --target=sparcel-unknown-elf
CFLAGS  := $(TARGET) -ffreestanding -nostdlib -O2 -Wall -Wextra -I$(CURDIR)/sdk/include -mcpu=v8

EXAMPLES := examples/hello examples/shell examples/selftest

all: check-headers $(EXAMPLES)

check-headers:
	$(CC) $(CFLAGS) -Wno-unused-function -fsyntax-only -x c sdk/include/npu3720_hw.h
	$(CC) $(CFLAGS) -Wno-unused-function -fsyntax-only -x c sdk/include/npu3720_sdk.h

$(EXAMPLES): check-headers
	$(MAKE) -C $@

clean:
	rm -f sdk/src/traps_table.S
	for d in $(EXAMPLES); do $(MAKE) -C $$d clean; done

.PHONY: all check-headers clean $(EXAMPLES)
