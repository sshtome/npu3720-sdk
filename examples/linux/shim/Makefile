ROOT    := ../../..
SDK_SRC := $(ROOT)/sdk/src
TOOLS   := $(ROOT)/tools

CC      := clang
CFLAGS  := --target=sparcel-unknown-elf -ffreestanding -nostdlib -O2 -Wall -Wextra \
           -I$(ROOT)/sdk/include -I. -mcpu=v8

SRCS_C  := main.c romvec.c $(SDK_SRC)/sdk_core.c $(SDK_SRC)/l2c.c $(SDK_SRC)/log.c \
           $(SDK_SRC)/ipc_jsm.c $(SDK_SRC)/timer.c $(SDK_SRC)/console.c $(SDK_SRC)/irq.c \
           $(SDK_SRC)/cpr.c $(SDK_SRC)/cmx.c
OBJS_C  := $(patsubst %.c,%.o,$(notdir $(SRCS_C)))
OBJS    := crt0.o lnn_crt0.o leonnn.o $(OBJS_C)

all: shim.bin

$(SDK_SRC)/traps_table.S: $(SDK_SRC)/gen_traps.py
	python3 $(SDK_SRC)/gen_traps.py $@

crt0.o: $(SDK_SRC)/crt0.S $(SDK_SRC)/traps_table.S
	$(CC) $(CFLAGS) -I$(SDK_SRC) -c $< -o $@

lnn_crt0.o: $(SDK_SRC)/lnn_crt0.S
	$(CC) $(CFLAGS) -I$(SDK_SRC) -c $< -o $@

leonnn.o: $(SDK_SRC)/leonnn.c
	$(CC) $(CFLAGS) -msoft-float -c $< -o $@

main.o: main.c romvec.h
	$(CC) $(CFLAGS) -c $< -o $@

romvec.o: romvec.c romvec.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: $(SDK_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

shim.bin: $(OBJS)
	python3 $(TOOLS)/link.py $(OBJS) -o $@ --base 0x84803000 --stack 0x84FF0000 --map shim.map

clean:
	rm -f *.o shim.bin shim.map

.PHONY: all clean
