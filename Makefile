# Source files to compile
SRCS=main.c gpu.c duart.c io.c audio.c pt3player.c rtc.c tk.c music_data.c

# Target CPU architecture
CPU=68000

# Cross-compiler prefix
# Use m68k-linux-gnu for Linux, m68k-eabi-elf for other systems
PREFIX=m68k-eabi-elf

# Build configuration
BUILDDIR=build
ROMDIR=roms

CC=$(PREFIX)-gcc
HOST_CC=gcc
LD=$(PREFIX)-ld
OBJCOPY=$(PREFIX)-objcopy
OBJDUMP=$(PREFIX)-objdump

# Auto-detect m68k_bare_metal path
ifndef m68kbm_PATH
ifneq ($(wildcard ../include/.*),)
    m68kbm_PATH=..
endif
ifneq ($(wildcard ../m68k_bare_metal/include/.*),)
    m68kbm_PATH=../m68k_bare_metal
endif
endif

CFLAGS=-m$(CPU) -Wall -Wextra -g -static -I${m68kbm_PATH}/include -I. -msoft-float -MMD -MP -O -std=c17
LFLAGS=--script=platform.ld -L${m68kbm_PATH}/libmetal -lmetal-$(CPU)

OBJS=$(patsubst %.c,$(BUILDDIR)/%.c.o,$(SRCS))
OBJS:=$(patsubst %.S,$(BUILDDIR)/%.S.o,$(OBJS))
OBJS:=$(patsubst %.s,$(BUILDDIR)/%.s.o,$(OBJS))
DEPS=$(OBJS:.o=.d)

# Generate music data from PT3 file
music_data.c: music/track.pt3
	xxd -i "$<" | sed 's/music_track_pt3/pt3player_main_track_pt3/g' > $@

font_bmp: font_bmp.c
	$(HOST_CC) -O2 -o font_bmp font_bmp.c

# BMP is source of truth — font.h is derived from it
font.h: font.bmp font_bmp
	./font_bmp bmp2h font.bmp font.h

.PHONY: all bmbinary release clean rom split distclean

# Default target: build everything
all: crt font.h bmbinary rom split

# Build the bare metal binary
bmbinary: $(OBJS) font.h crt
	$(LD) -o $(BUILDDIR)/$@ $(OBJS) $(LFLAGS)

# Release build with optimizations
release: CFLAGS+= -DNDEBUG
release: all

# Compile crt0.S startup code
crt: crt0.S
	$(CC) $(CFLAGS) -c -o crt0.o crt0.S
	rm -f crt0.d

# Create build directory
$(BUILDDIR):
	mkdir -p $@

# Compile C source files
$(BUILDDIR)/%.c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile assembly source files
$(BUILDDIR)/%.S.o: %.S
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.s.o: %.s
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEPS)

# Generate ROM files from binary
rom: bmbinary
	mkdir -p $(ROMDIR)
	$(OBJCOPY) -O binary $(BUILDDIR)/bmbinary $(ROMDIR)/bmbinary.rom
	$(OBJCOPY) -O srec $(BUILDDIR)/bmbinary $(ROMDIR)/bmbinary.srec

# Split ROM into two parts
split: rom
	cd $(ROMDIR) && \
	srec_cat bmbinary.rom -binary -split 2 0 -o rom.1.u2.bin -binary && \
	srec_cat bmbinary.rom -binary -split 2 1 -o rom.2.u6.bin -binary

# Build variants (all output to roms/ directory)
funlddlx: split
	cd $(ROMDIR) && \
	cp rom.1.u2.bin fldl_f6_2.bin && \
	cp rom.2.u6.bin fldl_f6_1.bin && \
	truncate --size=512K fldl_f6_1.bin && \
	truncate --size=512K fldl_f6_2.bin

funlddlxzip: funlddlx
	cd $(ROMDIR) && \
	zip -q funlddlx.zip fldl_f6_1.bin fldl_f6_2.bin

moneyf1: split
	cd $(ROMDIR) && \
	cp rom.1.u2.bin m27c1001_money_f1_ic1 && \
	cp rom.2.u6.bin m27c1001_money_f1_ic2 && \
	truncate --size=128K m27c1001_money_f1_ic1 && \
	truncate --size=128K m27c1001_money_f1_ic2

moneyf1zip: moneyf1
	cd $(ROMDIR) && \
	zip moneyf1.zip m27c1001_money_f1_ic1 m27c1001_money_f1_ic2

skattva: split
	cd $(ROMDIR) && \
	cp rom.1.u2.bin skat_tv_version_ts3.1.u2.bin && \
	cp rom.2.u6.bin skat_tv_version_ts3.2.u6.bin && \
	truncate --size=128K skat_tv_version_ts3.1.u2.bin && \
	truncate --size=128K skat_tv_version_ts3.2.u6.bin

skattvazip: skattva
	cd $(ROMDIR) && \
	zip skattva.zip skat_tv_version_ts3.1.u2.bin skat_tv_version_ts3.2.u6.bin

skattv: split
	cd $(ROMDIR) && \
	cp rom.1.u2.bin f2_i.bin && \
	cp rom.2.u6.bin f2_ii.bin && \
	cp rom.1.u2.bin f1_i.bin && \
	cp rom.2.u6.bin f1_ii.bin && \
	truncate --size=128K f2_i.bin && \
	truncate --size=128K f2_ii.bin && \
	truncate --size=128K f1_i.bin && \
	truncate --size=128K f1_ii.bin

skattvzip: skattv
	cd $(ROMDIR) && \
	zip skattv.zip f2_i.bin f2_ii.bin f1_i.bin f1_ii.bin

burn: split
	cd $(ROMDIR) && \
	truncate --size=64K rom.1.u2.bin && \
	truncate --size=64K rom.2.u6.bin && \
	cat rom.2.u6.bin rom.2.u6.bin rom.2.u6.bin rom.2.u6.bin rom.2.u6.bin rom.2.u6.bin rom.2.u6.bin rom.2.u6.bin > rom.2.expanded.u6.bin && \
	cat rom.1.u2.bin rom.1.u2.bin rom.1.u2.bin rom.1.u2.bin rom.1.u2.bin rom.1.u2.bin rom.1.u2.bin rom.1.u2.bin > rom.1.expanded.u2.bin && \
	minipro -p W27E040 -w rom.1.expanded.u2.bin && \
	read -n1 -r -p "Swap the ROM and press any key..." key && \
	minipro -p W27E040 -w rom.2.expanded.u6.bin

testr: funlddlxzip
	cp $(ROMDIR)/funlddlx.zip /run/media/stoned/schrott/Roms/mame/roms/
	cd /run/media/stoned/schrott/msys64/src/mame/
	/run/media/stoned/schrott/msys64/src/mame/mame funlddlx -rompath /run/media/stoned/schrott/Roms/mame/roms/ -window

test: skattvzip
	cp $(ROMDIR)/skattv.zip /run/media/stoned/schrott/Roms/mame/roms/
	cd /run/media/stoned/schrott/msys64/src/mame/
	/run/media/stoned/schrott/msys64/src/mame/mame skattv -rompath /run/media/stoned/schrott/Roms/mame/roms/ -window -debug

testn: moneyf1zip
	cp $(ROMDIR)/moneyf1.zip /run/media/stoned/schrott/Roms/mame/roms/
	cd /run/media/stoned/schrott/msys64/src/mame/
	/run/media/stoned/schrott/msys64/src/mame/mame moneyf1 -rompath /run/media/stoned/schrott/Roms/mame/roms/ -window -debug


# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)/*
	rm -f music_data.c crt0.o crt0.d

# Clean everything including ROMs
distclean: clean
	rm -rf $(ROMDIR)/*

# Debug targets
dump:
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.evt $(BUILDDIR)/bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -dt -j.text $(BUILDDIR)/bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.rodata -j.data -j.bss -j.heap -j.stack $(BUILDDIR)/bmbinary

dumps:
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.evt $(BUILDDIR)/bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -St -j.text $(BUILDDIR)/bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.rodata -j.data -j.bss -j.heap -j.stack $(BUILDDIR)/bmbinary

hexdump:
	hexdump -C $(ROMDIR)/bmbinary.rom
