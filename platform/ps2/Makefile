# SDK conventions verified against ps2dev/ps2sdk; see docs/ps2.md.
ifeq ($(strip $(PS2SDK)),)
$(error PS2SDK is unset; see docs/ps2.md)
endif
EE_BIN = mipsnes2-ps2.elf
EE_OBJS = main.o diagnostic_suite.o rom_file.o video_frame.o gs_present.o log.o bus.o cpu_io.o apu_io_ports.o pcm.o dsp.o smp_timers.o smp_memory.o spc700_cpu.o apu_timing.o dma_registers.o dma_transfer.o hdma.o ppu_counters.o ppu_vram.o ppu_cgram.o ppu_oam.o ppu_tile4.o ppu_sprite_scanline.o ppu_bg2_scanline.o ppu_bg4_scanline.o ppu_bg_registers.o ppu_mode0_mix.o ppu_mode1_mix.o ppu_window_mask.o ppu_color_math.o snes_bus.o cpu.o scheduler.o 5a22_timing.o \
	hv_timing.o interrupt_lines.o master_timing.o cpu_timing.o cartridge.o \
	cartridge_image.o \
	cartridge_prepare.o machine.o
EE_LIBS = -ldraw -lgraph -lpacket -ldma -ldebug -lc
EE_INCS = -I../..
EE_CFLAGS = -std=c99 -Wextra -Werror

.PHONY: all clean
all: $(EE_BIN)

video_frame.o: video_frame.c video_frame.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

gs_present.o: gs_present.c gs_present.h video_frame.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

log.o: ../../core/common/log.c ../../core/common/log.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

bus.o: ../../core/bus/bus.c ../../core/bus/bus.h ../../core/bus/interface.h ../../core/common/address.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cpu_io.o: ../../core/bus/cpu_io.c ../../core/bus/cpu_io.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

apu_io_ports.o: ../../core/apu/io_ports.c ../../core/apu/io_ports.h ../../core/bus/interface.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

pcm.o: ../../core/apu/pcm.c ../../core/apu/pcm.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

dsp.o: ../../core/apu/dsp/dsp.c ../../core/apu/dsp/dsp.h ../../core/apu/pcm.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

smp_timers.o: ../../core/apu/spc700/timers.c ../../core/apu/spc700/timers.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

smp_memory.o: ../../core/apu/spc700/memory.c ../../core/apu/spc700/memory.h ../../core/apu/spc700/timers.h ../../core/apu/io_ports.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

spc700_cpu.o: ../../core/apu/spc700/cpu.c ../../core/apu/spc700/cpu.h ../../core/apu/spc700/memory.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

apu_timing.o: ../../core/apu/timing.c ../../core/apu/timing.h ../../core/apu/dsp/dsp.h ../../core/apu/spc700/cpu.h ../../core/scheduler/scheduler.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

dma_registers.o: ../../core/dma/registers.c ../../core/dma/registers.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

dma_transfer.o: ../../core/dma/transfer.c ../../core/dma/transfer.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

hdma.o: ../../core/dma/hdma.c ../../core/dma/hdma.h ../../core/dma/transfer.h
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

ppu_bg4_scanline.o: ../../core/ppu/bg4_scanline.c ../../core/ppu/bg4_scanline.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_bg_registers.o: ../../core/ppu/bg_registers.c ../../core/ppu/bg_registers.h ../../core/ppu/bg2_scanline.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_mode0_mix.o: ../../core/ppu/mode0_mix.c ../../core/ppu/mode0_mix.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_mode1_mix.o: ../../core/ppu/mode1_mix.c ../../core/ppu/mode1_mix.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_window_mask.o: ../../core/ppu/window_mask.c ../../core/ppu/window_mask.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

ppu_color_math.o: ../../core/ppu/color_math.c ../../core/ppu/color_math.h
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
