# SDK conventions verified against pspdev/pspsdk; see docs/psp.md.
TARGET = mipsnes2-psp
OBJS = main.o log.o bus.o cpu_io.o ppu_counters.o ppu_vram.o ppu_cgram.o ppu_oam.o ppu_tile4.o ppu_sprite_scanline.o ppu_bg2_scanline.o ppu_bg4_scanline.o ppu_bg_registers.o ppu_mode0_mix.o ppu_mode1_mix.o ppu_window_mask.o ppu_color_math.o snes_bus.o cpu.o scheduler.o 5a22_timing.o \
	hv_timing.o interrupt_lines.o master_timing.o cpu_timing.o cartridge.o \
	cartridge_image.o \
	cartridge_prepare.o machine.o
BUILD_PRX = 1
INCDIR = ../..
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = MIPSNES2 CPU Foundation - No ROMs
PSPSDK := $(shell psp-config --pspsdk-path)
ifeq ($(strip $(PSPSDK)),)
$(error psp-config unavailable; see docs/psp.md)
endif

.PHONY: all
all: $(EXTRA_TARGETS)

log.o: ../../core/common/log.c ../../core/common/log.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

bus.o: ../../core/bus/bus.c ../../core/bus/bus.h ../../core/bus/interface.h ../../core/common/address.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cpu_io.o: ../../core/bus/cpu_io.c ../../core/bus/cpu_io.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_counters.o: ../../core/ppu/counters.c ../../core/ppu/counters.h ../../core/scheduler/hv_timing.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_vram.o: ../../core/ppu/vram_ports.c ../../core/ppu/vram_ports.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_cgram.o: ../../core/ppu/cgram_ports.c ../../core/ppu/cgram_ports.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_oam.o: ../../core/ppu/oam_ports.c ../../core/ppu/oam_ports.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_tile4.o: ../../core/ppu/tile4.c ../../core/ppu/tile4.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_sprite_scanline.o: ../../core/ppu/sprite_scanline.c ../../core/ppu/sprite_scanline.h ../../core/ppu/oam_ports.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_bg2_scanline.o: ../../core/ppu/bg2_scanline.c ../../core/ppu/bg2_scanline.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_bg4_scanline.o: ../../core/ppu/bg4_scanline.c ../../core/ppu/bg4_scanline.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_bg_registers.o: ../../core/ppu/bg_registers.c ../../core/ppu/bg_registers.h ../../core/ppu/bg2_scanline.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_mode0_mix.o: ../../core/ppu/mode0_mix.c ../../core/ppu/mode0_mix.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_mode1_mix.o: ../../core/ppu/mode1_mix.c ../../core/ppu/mode1_mix.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_window_mask.o: ../../core/ppu/window_mask.c ../../core/ppu/window_mask.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

ppu_color_math.o: ../../core/ppu/color_math.c ../../core/ppu/color_math.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

snes_bus.o: ../../core/bus/snes_bus.c ../../core/bus/snes_bus.h ../../core/bus/cpu_io.h ../../core/bus/interface.h ../../core/cartridge/interface.h ../../core/common/address.h ../../core/scheduler/5a22_timing.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cpu.o: ../../core/cpu/65c816/cpu.c ../../core/cpu/65c816/cpu.h ../../core/bus/interface.h ../../core/common/address.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

scheduler.o: ../../core/scheduler/scheduler.c ../../core/scheduler/scheduler.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

5a22_timing.o: ../../core/scheduler/5a22_timing.c ../../core/scheduler/5a22_timing.h ../../core/common/address.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

hv_timing.o: ../../core/scheduler/hv_timing.c ../../core/scheduler/hv_timing.h ../../core/bus/cpu_io.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

interrupt_lines.o: ../../core/scheduler/interrupt_lines.c ../../core/scheduler/interrupt_lines.h ../../core/bus/cpu_io.h ../../core/cpu/65c816/cpu.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

master_timing.o: ../../core/scheduler/master_timing.c ../../core/scheduler/master_timing.h ../../core/scheduler/hv_timing.h ../../core/scheduler/scheduler.h ../../core/bus/cpu_io.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cpu_timing.o: ../../core/scheduler/cpu_timing.c ../../core/scheduler/cpu_timing.h ../../core/scheduler/master_timing.h ../../core/scheduler/interrupt_lines.h ../../core/scheduler/scheduler.h ../../core/scheduler/hv_timing.h ../../core/bus/snes_bus.h ../../core/cpu/65c816/cpu.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cartridge.o: ../../core/cartridge/cartridge.c ../../core/cartridge/cartridge.h ../../core/cartridge/interface.h ../../core/common/address.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cartridge_image.o: ../../core/cartridge/image.c ../../core/cartridge/image.h ../../core/cartridge/cartridge.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cartridge_prepare.o: ../../core/cartridge/prepare.c ../../core/cartridge/prepare.h ../../core/cartridge/image.h ../../core/cartridge/cartridge.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

machine.o: ../../core/machine/machine.c ../../core/machine/machine.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

main.o: main.c ../../core/common/log.h ../../core/common/project.h

include $(PSPSDK)/lib/build.mak
