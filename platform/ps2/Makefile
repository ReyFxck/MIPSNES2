# SDK conventions verified against ps2dev/ps2sdk; see docs/ps2.md.
ifeq ($(strip $(PS2SDK)),)
$(error PS2SDK is unset; see docs/ps2.md)
endif
EE_BIN = mipsnes2-ps2.elf
EE_OBJS = main.o diagnostic_suite.o rom_file.o log.o bus.o cpu_io.o ppu_counters.o ppu_vram.o ppu_cgram.o ppu_oam.o ppu_tile4.o ppu_sprite_scanline.o ppu_bg2_scanline.o snes_bus.o cpu.o scheduler.o 5a22_timing.o \
	hv_timing.o interrupt_lines.o master_timing.o cpu_timing.o cartridge.o \
	cartridge_image.o \
	cartridge_prepare.o machine.o
EE_LIBS = -ldebug -lc
EE_INCS = -I../..
EE_CFLAGS = -std=c99 -Wextra -Werror

.PHONY: all clean
all: $(EE_BIN)

log.o: ../../core/common/log.c ../../core/common/log.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

bus.o: ../../core/bus/bus.c ../../core/bus/bus.h ../../core/bus/interface.h ../../core/common/address.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cpu_io.o: ../../core/bus/cpu_io.c ../../core/bus/cpu_io.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_counters.o: ../../core/ppu/counters.c ../../core/ppu/counters.h ../../core/scheduler/hv_timing.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_vram.o: ../../core/ppu/vram_ports.c ../../core/ppu/vram_ports.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_cgram.o: ../../core/ppu/cgram_ports.c ../../core/ppu/cgram_ports.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_oam.o: ../../core/ppu/oam_ports.c ../../core/ppu/oam_ports.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_tile4.o: ../../core/ppu/tile4.c ../../core/ppu/tile4.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_sprite_scanline.o: ../../core/ppu/sprite_scanline.c ../../core/ppu/sprite_scanline.h ../../core/ppu/oam_ports.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_bg2_scanline.o: ../../core/ppu/bg2_scanline.c ../../core/ppu/bg2_scanline.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

snes_bus.o: ../../core/bus/snes_bus.c ../../core/bus/snes_bus.h ../../core/bus/cpu_io.h ../../core/bus/interface.h ../../core/cartridge/interface.h ../../core/common/address.h ../../core/scheduler/5a22_timing.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cpu.o: ../../core/cpu/65c816/cpu.c ../../core/cpu/65c816/cpu.h ../../core/bus/interface.h ../../core/common/address.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

scheduler.o: ../../core/scheduler/scheduler.c ../../core/scheduler/scheduler.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

5a22_timing.o: ../../core/scheduler/5a22_timing.c ../../core/scheduler/5a22_timing.h ../../core/common/address.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

hv_timing.o: ../../core/scheduler/hv_timing.c ../../core/scheduler/hv_timing.h ../../core/bus/cpu_io.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

interrupt_lines.o: ../../core/scheduler/interrupt_lines.c ../../core/scheduler/interrupt_lines.h ../../core/bus/cpu_io.h ../../core/cpu/65c816/cpu.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

master_timing.o: ../../core/scheduler/master_timing.c ../../core/scheduler/master_timing.h ../../core/scheduler/hv_timing.h ../../core/scheduler/scheduler.h ../../core/bus/cpu_io.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cpu_timing.o: ../../core/scheduler/cpu_timing.c ../../core/scheduler/cpu_timing.h ../../core/scheduler/master_timing.h ../../core/scheduler/interrupt_lines.h ../../core/scheduler/scheduler.h ../../core/scheduler/hv_timing.h ../../core/bus/snes_bus.h ../../core/cpu/65c816/cpu.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cartridge.o: ../../core/cartridge/cartridge.c ../../core/cartridge/cartridge.h ../../core/cartridge/interface.h ../../core/common/address.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cartridge_image.o: ../../core/cartridge/image.c ../../core/cartridge/image.h ../../core/cartridge/cartridge.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cartridge_prepare.o: ../../core/cartridge/prepare.c ../../core/cartridge/prepare.h ../../core/cartridge/image.h ../../core/cartridge/cartridge.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

machine.o: ../../core/machine/machine.c ../../core/machine/machine.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

main.o: main.c diagnostic_suite.h

diagnostic_suite.o: diagnostic_suite.c diagnostic_suite.h ../../core/bus/snes_bus.h ../../core/cpu/65c816/cpu.h ../../core/scheduler/cpu_timing.h

rom_file.o: rom_file.c rom_file.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
