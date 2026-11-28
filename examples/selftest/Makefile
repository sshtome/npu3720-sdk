ROOT    := ../..
SDK_SRC := $(ROOT)/sdk/src
TOOLS   := $(ROOT)/tools

CC      := clang
CFLAGS  := --target=sparcel-unknown-elf -ffreestanding -nostdlib -O2 -Wall -Wextra \
           -I$(ROOT)/sdk/include -mcpu=v8

SRCS_C  := main.c $(SDK_SRC)/sdk_core.c $(SDK_SRC)/l2c.c $(SDK_SRC)/log.c \
           $(SDK_SRC)/ipc_jsm.c $(SDK_SRC)/timer.c $(SDK_SRC)/console.c $(SDK_SRC)/irq.c \
           $(SDK_SRC)/cpr.c $(SDK_SRC)/cmx.c $(SDK_SRC)/dma.c $(SDK_SRC)/mmu.c
OBJS_C  := $(patsubst %.c,%.o,$(notdir $(SRCS_C)))
OBJS    := crt0.o lnn_crt0.o leonnn.o $(OBJS_C)

all: vpu_37xx_selftest.bin

$(SDK_SRC)/traps_table.S: $(SDK_SRC)/gen_traps.py
	python3 $(SDK_SRC)/gen_traps.py $@

crt0.o: $(SDK_SRC)/crt0.S $(SDK_SRC)/traps_table.S
	$(CC) $(CFLAGS) -I$(SDK_SRC) -c $< -o $@

lnn_crt0.o: $(SDK_SRC)/lnn_crt0.S
	$(CC) $(CFLAGS) -I$(SDK_SRC) -c $< -o $@

leonnn.o: $(SDK_SRC)/leonnn.c
	$(CC) $(CFLAGS) -msoft-float -c $< -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: $(SDK_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

fw.bin: $(OBJS)
	python3 $(TOOLS)/link.py $(OBJS) -o $@ --map fw.map

vpu_37xx_selftest.bin: fw.bin
	python3 $(TOOLS)/pack_fw.py $< -o $@ --version "npu3720-sdk-selftest 0.2.0"

clean:
	rm -f *.o fw.bin fw.map vpu_37xx_selftest.bin

.PHONY: all clean
