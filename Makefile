# List all of the source files that will be compiled into your binary.
#
# For example, if you have the following source files
#
#   main.c
#   user.c
#   driver.s
#
# then your SRCS list would be
#
#   SRCS=main.c user.c driver.s
#
# The list may span several lines of text by appending a backslash to each line,
# for example:
#
#   SRCS=main.c user.c \
#        driver.s
SRCS=main.c gpu.c duart.c io.c audio.c pt3player.c music_data.c

# Specify the CPU type that you are targeting your build towards.
#
# Supported architectures can usually be found with the --target-help argument
# passed to gcc, but a quick summary is:
#
# 68000, 68010, 68020, 68030, 68040, 68060, cpu32 (includes 68332 and 68360),
# 68302
CPU=68000

# Uncomment either of the following depending on how you have installed gcc on
# your system. m68k-linux-gnu for Linux installations, m68k-eabi-elf if gcc was
# built from scratch e.g. on a Mac by running the build script.
# PREFIX=m68k-linux-gnu
PREFIX=m68k-eabi-elf

# Dont modify below this line (unless you know what youre doing).
BUILDDIR=build

CC=$(PREFIX)-gcc
LD=$(PREFIX)-ld
OBJCOPY=$(PREFIX)-objcopy
OBJDUMP=$(PREFIX)-objdump

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

music_data.c: music/track.pt3
	xxd -i "$<" | sed 's/music_track_pt3/pt3player_main_track_pt3/g' > $@

.PHONY: bmbinary release all crt clean rom dump dumps hexdump

bmbinary: $(OBJS) crt
	$(LD) -o $@ $(OBJS) $(LFLAGS)

release: CFLAGS+= -DNDEBUG
release: all

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/%.c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.S.o: %.S
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.s.o: %.s
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEPS)

all: crt bmbinary rom split

crt: crt0.S
	$(CC) $(CFLAGS) -c -o crt0.o crt0.S
	rm -f crt0.d

clean:
	rm -rf $(BUILDDIR)/*
	rm -f bmbinary*
	rm -f music_data.c

rom: bmbinary
	$(OBJCOPY) -O binary bmbinary bmbinary.rom
	$(OBJCOPY) -O srec bmbinary bmbinary.srec

split: rom
	srec_cat bmbinary.rom -binary -split 2 0 -o rom.1.u2.bin -binary
	srec_cat bmbinary.rom -binary -split 2 1 -o rom.2.u6.bin -binary

funlddlx: split
	cp rom.1.u2.bin fldl_f6_2.bin
	cp rom.2.u6.bin fldl_f6_1.bin
	truncate --size=512K fldl_f6_1.bin
	truncate --size=512K fldl_f6_2.bin
	7z a funlddlx.zip fldl_f6_1.bin fldl_f6_2.bin
	rm fldl_f6_1.bin fldl_f6_2.bin

moneyf1: split
	cp rom.1.u2.bin m27c1001_money_f1_ic1
	cp rom.2.u6.bin m27c1001_money_f1_ic2
	truncate --size=128K m27c1001_money_f1_ic1
	truncate --size=128K m27c1001_money_f1_ic2
	7z a moneyf1.zip m27c1001_money_f1_ic1 m27c1001_money_f1_ic2
	rm m27c1001_money_f1_ic1 m27c1001_money_f1_ic2

skattva: split
	cp rom.1.u2.bin skat_tv_version_ts3.1.u2.bin
	cp rom.2.u6.bin skat_tv_version_ts3.2.u6.bin
	truncate --size=128K skat_tv_version_ts3.1.u2.bin
	truncate --size=128K skat_tv_version_ts3.2.u6.bin
	7z a skattva.zip skat_tv_version_ts3.1.u2.bin skat_tv_version_ts3.2.u6.bin
	rm skat_tv_version_ts3.1.u2.bin skat_tv_version_ts3.2.u6.bin

skattv: split
	cp rom.1.u2.bin f2_i.bin
	cp rom.2.u6.bin f2_ii.bin
	cp rom.1.u2.bin f1_i.bin
	cp rom.2.u6.bin f1_ii.bin
	truncate --size=128K f2_i.bin
	truncate --size=128K f2_ii.bin
	truncate --size=128K f1_i.bin
	truncate --size=128K f1_ii.bin
	7z a skattv.zip f2_i.bin f2_ii.bin f1_i.bin f1_ii.bin
	rm f2_i.bin f2_ii.bin f1_i.bin f1_ii.bin

burn: split
	truncate --size=512K rom.1.u2.bin
	minipro -p W27E040 -w rom.1.u2.bin
	read -n1 -r -p "Swap the ROM and press any key..." key
	truncate --size=512K rom.2.u6.bin
	minipro -p W27E040 -w rom.2.u6.bin

testr: funlddlx
	cp funlddlx.zip /run/media/stoned/schrott/Roms/mame/roms/
	cd /run/media/stoned/schrott/msys64/src/mame/
	/run/media/stoned/schrott/msys64/src/mame/mame funlddlx -rompath /run/media/stoned/schrott/Roms/mame/roms/ -window

test: skattv
	cp skattv.zip /run/media/stoned/schrott/Roms/mame/roms/
	cd /run/media/stoned/schrott/msys64/src/mame/
	/run/media/stoned/schrott/msys64/src/mame/mame skattv -rompath /run/media/stoned/schrott/Roms/mame/roms/ -window

testn: moneyf1
	cp moneyf1.zip /run/media/stoned/schrott/Roms/mame/roms/
	cd /run/media/stoned/schrott/msys64/src/mame/
	/run/media/stoned/schrott/msys64/src/mame/mame moneyf1 -rompath /run/media/stoned/schrott/Roms/mame/roms/ -window -debug

dump:
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.evt bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -dt -j.text bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.rodata -j.data -j.bss -j.heap -j.stack bmbinary

dumps:
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.evt bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -St -j.text bmbinary
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.rodata -j.data -j.bss -j.heap -j.stack bmbinary

hexdump:
	hexdump -C bmbinary.rom
