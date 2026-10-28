# Host-only foundation build. Console SDK builds stay in platform/.
CC ?= cc
AR ?= ar
PYTHON ?= python3
BUILD_DIR ?= build/host
CPPFLAGS ?=
CFLAGS ?= -O2 -g
LDFLAGS ?=
LDLIBS ?=
WARNINGS = -std=c99 -Wall -Wextra -Wpedantic -Werror
INCLUDES = -I.
LIB = $(BUILD_DIR)/libmipsnes2-core.a
HOST = $(BUILD_DIR)/mipsnes2-host
TEST = $(BUILD_DIR)/test-contracts
TEST_NOLOG = $(BUILD_DIR)/test-contracts-nolog
TEST_CPU_BUS = $(BUILD_DIR)/test-cpu-bus
TEST_CPU_CYCLES = $(BUILD_DIR)/test-cpu-cycles
TEST_CPU_DIFFERENTIAL = $(BUILD_DIR)/test-cpu-differential
TEST_CPU_IO = $(BUILD_DIR)/test-cpu-io
TEST_APU_IO_PORTS = $(BUILD_DIR)/test-apu-io-ports
TEST_PCM = $(BUILD_DIR)/test-pcm
TEST_DSP = $(BUILD_DIR)/test-dsp
TEST_APU_TIMING = $(BUILD_DIR)/test-apu-timing
TEST_SMP_TIMERS = $(BUILD_DIR)/test-smp-timers
TEST_SMP_MEMORY = $(BUILD_DIR)/test-smp-memory
TEST_SPC700_CPU = $(BUILD_DIR)/test-spc700-cpu
TEST_SPC700_ALU = $(BUILD_DIR)/test-spc700-alu
TEST_SPC700_COMPLETE = $(BUILD_DIR)/test-spc700-complete
TEST_PPU_COUNTERS = $(BUILD_DIR)/test-ppu-counters
TEST_PPU_VRAM = $(BUILD_DIR)/test-ppu-vram
TEST_PPU_CGRAM = $(BUILD_DIR)/test-ppu-cgram
TEST_PPU_OAM = $(BUILD_DIR)/test-ppu-oam
TEST_PPU_TILE4 = $(BUILD_DIR)/test-ppu-tile4
TEST_PPU_SPRITE_SCANLINE = $(BUILD_DIR)/test-ppu-sprite-scanline
TEST_PPU_BG2_SCANLINE = $(BUILD_DIR)/test-ppu-bg2-scanline
TEST_PPU_BG4_SCANLINE = $(BUILD_DIR)/test-ppu-bg4-scanline
TEST_PPU_BG_REGISTERS = $(BUILD_DIR)/test-ppu-bg-registers
TEST_PPU_MODE0_MIX = $(BUILD_DIR)/test-ppu-mode0-mix
TEST_PPU_MODE1_MIX = $(BUILD_DIR)/test-ppu-mode1-mix
TEST_PPU_WINDOW_MASK = $(BUILD_DIR)/test-ppu-window-mask
TEST_PPU_COLOR_MATH = $(BUILD_DIR)/test-ppu-color-math
TEST_HV_TIMING = $(BUILD_DIR)/test-hv-timing
TEST_INTERRUPT_LINES = $(BUILD_DIR)/test-interrupt-lines
TEST_MASTER_TIMING = $(BUILD_DIR)/test-master-timing
TEST_SCHEDULER = $(BUILD_DIR)/test-scheduler
TEST_SNES_BUS = $(BUILD_DIR)/test-snes-bus
TEST_CARTRIDGE = $(BUILD_DIR)/test-cartridge
TEST_CARTRIDGE_IMAGE = $(BUILD_DIR)/test-cartridge-image
TEST_CARTRIDGE_PREPARE = $(BUILD_DIR)/test-cartridge-prepare
TEST_PS2_DIAGNOSTIC = $(BUILD_DIR)/test-ps2-diagnostic
TEST_MACHINE = $(BUILD_DIR)/test-machine
TEST_PS2_ROM_FILE = $(BUILD_DIR)/test-ps2-rom-file
TEST_PS2_VIDEO_FRAME = $(BUILD_DIR)/test-ps2-video-frame
TEST_DMA_REGISTERS = $(BUILD_DIR)/test-dma-registers
TEST_DMA_TRANSFER = $(BUILD_DIR)/test-dma-transfer
TEST_HDMA = $(BUILD_DIR)/test-hdma
TEST_DMA_TIMING = $(BUILD_DIR)/test-dma-timing
CORE_OBJS = $(BUILD_DIR)/log.o $(BUILD_DIR)/bus.o $(BUILD_DIR)/cpu_io.o $(BUILD_DIR)/apu_io_ports.o $(BUILD_DIR)/pcm.o $(BUILD_DIR)/dsp.o $(BUILD_DIR)/smp_timers.o $(BUILD_DIR)/smp_memory.o $(BUILD_DIR)/spc700_cpu.o $(BUILD_DIR)/apu_timing.o $(BUILD_DIR)/dma_registers.o $(BUILD_DIR)/dma_transfer.o $(BUILD_DIR)/hdma.o $(BUILD_DIR)/ppu_counters.o $(BUILD_DIR)/ppu_vram.o $(BUILD_DIR)/ppu_cgram.o $(BUILD_DIR)/ppu_oam.o $(BUILD_DIR)/ppu_tile4.o $(BUILD_DIR)/ppu_sprite_scanline.o $(BUILD_DIR)/ppu_bg2_scanline.o $(BUILD_DIR)/ppu_bg4_scanline.o $(BUILD_DIR)/ppu_bg_registers.o $(BUILD_DIR)/ppu_mode0_mix.o $(BUILD_DIR)/ppu_mode1_mix.o $(BUILD_DIR)/ppu_window_mask.o $(BUILD_DIR)/ppu_color_math.o \
	$(BUILD_DIR)/cpu.o \
	$(BUILD_DIR)/snes_bus.o $(BUILD_DIR)/scheduler.o \
	$(BUILD_DIR)/5a22_timing.o $(BUILD_DIR)/hv_timing.o \
	$(BUILD_DIR)/interrupt_lines.o \
	$(BUILD_DIR)/master_timing.o \
	$(BUILD_DIR)/cpu_timing.o \
	$(BUILD_DIR)/cartridge.o $(BUILD_DIR)/cartridge_image.o \
	$(BUILD_DIR)/cartridge_prepare.o $(BUILD_DIR)/machine.o

.PHONY: all test check clean
all: $(HOST)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/log.o: core/common/log.c core/common/log.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/bus.o: core/bus/bus.c core/bus/bus.h core/bus/interface.h core/common/address.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cpu_io.o: core/bus/cpu_io.c core/bus/cpu_io.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/apu_io_ports.o: core/apu/io_ports.c core/apu/io_ports.h core/bus/interface.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/pcm.o: core/apu/pcm.c core/apu/pcm.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/dsp.o: core/apu/dsp/dsp.c core/apu/dsp/dsp.h core/apu/pcm.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/smp_timers.o: core/apu/spc700/timers.c core/apu/spc700/timers.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/smp_memory.o: core/apu/spc700/memory.c core/apu/spc700/memory.h core/apu/spc700/timers.h core/apu/io_ports.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/spc700_cpu.o: core/apu/spc700/cpu.c core/apu/spc700/cpu.h core/apu/spc700/memory.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/apu_timing.o: core/apu/timing.c core/apu/timing.h core/apu/dsp/dsp.h core/apu/spc700/cpu.h core/scheduler/scheduler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/dma_registers.o: core/dma/registers.c core/dma/registers.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/dma_transfer.o: core/dma/transfer.c core/dma/transfer.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/hdma.o: core/dma/hdma.c core/dma/hdma.h core/dma/transfer.h core/dma/registers.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_counters.o: core/ppu/counters.c core/ppu/counters.h core/scheduler/hv_timing.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_vram.o: core/ppu/vram_ports.c core/ppu/vram_ports.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_cgram.o: core/ppu/cgram_ports.c core/ppu/cgram_ports.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_oam.o: core/ppu/oam_ports.c core/ppu/oam_ports.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_tile4.o: core/ppu/tile4.c core/ppu/tile4.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_sprite_scanline.o: core/ppu/sprite_scanline.c core/ppu/sprite_scanline.h core/ppu/oam_ports.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_bg2_scanline.o: core/ppu/bg2_scanline.c core/ppu/bg2_scanline.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_bg4_scanline.o: core/ppu/bg4_scanline.c core/ppu/bg4_scanline.h core/ppu/bg2_scanline.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_bg_registers.o: core/ppu/bg_registers.c core/ppu/bg_registers.h core/ppu/bg2_scanline.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_mode0_mix.o: core/ppu/mode0_mix.c core/ppu/mode0_mix.h core/ppu/bg2_scanline.h core/ppu/sprite_scanline.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_mode1_mix.o: core/ppu/mode1_mix.c core/ppu/mode1_mix.h core/ppu/mode0_mix.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_window_mask.o: core/ppu/window_mask.c core/ppu/window_mask.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/ppu_color_math.o: core/ppu/color_math.c core/ppu/color_math.h core/ppu/mode0_mix.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/snes_bus.o: core/bus/snes_bus.c core/bus/snes_bus.h core/bus/cpu_io.h core/bus/interface.h core/cartridge/interface.h core/common/address.h core/scheduler/5a22_timing.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cpu.o: core/cpu/65c816/cpu.c core/cpu/65c816/cpu.h core/bus/interface.h core/common/address.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/scheduler.o: core/scheduler/scheduler.c core/scheduler/scheduler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/5a22_timing.o: core/scheduler/5a22_timing.c core/scheduler/5a22_timing.h core/common/address.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/hv_timing.o: core/scheduler/hv_timing.c core/scheduler/hv_timing.h core/bus/cpu_io.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/interrupt_lines.o: core/scheduler/interrupt_lines.c core/scheduler/interrupt_lines.h core/bus/cpu_io.h core/cpu/65c816/cpu.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/master_timing.o: core/scheduler/master_timing.c core/scheduler/master_timing.h core/scheduler/hv_timing.h core/scheduler/scheduler.h core/bus/cpu_io.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cpu_timing.o: core/scheduler/cpu_timing.c core/scheduler/cpu_timing.h core/scheduler/master_timing.h core/scheduler/interrupt_lines.h core/scheduler/scheduler.h core/scheduler/hv_timing.h core/bus/snes_bus.h core/cpu/65c816/cpu.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cartridge.o: core/cartridge/cartridge.c core/cartridge/cartridge.h core/cartridge/interface.h core/common/address.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cartridge_image.o: core/cartridge/image.c core/cartridge/image.h core/cartridge/cartridge.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cartridge_prepare.o: core/cartridge/prepare.c core/cartridge/prepare.h core/cartridge/image.h core/cartridge/cartridge.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/machine.o: core/machine/machine.c core/machine/machine.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(LIB): $(CORE_OBJS)
	$(AR) rcs $@ $^

$(HOST): platform/host/main.c core/common/log.h core/common/project.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST): tests/test_contracts.c core/common/address.h core/common/log.h core/bus/interface.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_NOLOG): tests/test_contracts.c core/common/log.c core/common/log.h core/common/address.h core/bus/interface.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -DMIPSNES2_ENABLE_LOGGING=0 tests/test_contracts.c core/common/log.c $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CPU_BUS): tests/test_cpu_bus.c core/bus/bus.h core/cpu/65c816/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CPU_CYCLES): tests/test_cpu_cycles.c core/cpu/65c816/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CPU_DIFFERENTIAL): tests/test_cpu_differential.c core/cpu/65c816/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CPU_IO): tests/test_cpu_io.c core/bus/cpu_io.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_APU_IO_PORTS): tests/test_apu_io_ports.c core/apu/io_ports.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PCM): tests/test_pcm.c core/apu/pcm.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DSP): tests/test_dsp.c core/apu/dsp/dsp.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_APU_TIMING): tests/test_apu_timing.c core/apu/timing.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SMP_TIMERS): tests/test_smp_timers.c core/apu/spc700/timers.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SMP_MEMORY): tests/test_smp_memory.c core/apu/spc700/memory.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SPC700_CPU): tests/test_spc700_cpu.c core/apu/spc700/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SPC700_ALU): tests/test_spc700_alu.c core/apu/spc700/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SPC700_COMPLETE): tests/test_spc700_complete.c core/apu/spc700/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_COUNTERS): tests/test_ppu_counters.c core/ppu/counters.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_VRAM): tests/test_ppu_vram.c core/ppu/vram_ports.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_CGRAM): tests/test_ppu_cgram.c core/ppu/cgram_ports.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_OAM): tests/test_ppu_oam.c core/ppu/oam_ports.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_TILE4): tests/test_ppu_tile4.c core/ppu/tile4.h core/ppu/oam_ports.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_SPRITE_SCANLINE): tests/test_ppu_sprite_scanline.c core/ppu/sprite_scanline.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_BG2_SCANLINE): tests/test_ppu_bg2_scanline.c core/ppu/bg2_scanline.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_BG4_SCANLINE): tests/test_ppu_bg4_scanline.c core/ppu/bg4_scanline.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_BG_REGISTERS): tests/test_ppu_bg_registers.c core/ppu/bg_registers.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_MODE0_MIX): tests/test_ppu_mode0_mix.c core/ppu/mode0_mix.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_MODE1_MIX): tests/test_ppu_mode1_mix.c core/ppu/mode1_mix.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_WINDOW_MASK): tests/test_ppu_window_mask.c core/ppu/window_mask.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PPU_COLOR_MATH): tests/test_ppu_color_math.c core/ppu/color_math.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_HV_TIMING): tests/test_hv_timing.c core/scheduler/hv_timing.h core/bus/cpu_io.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_INTERRUPT_LINES): tests/test_interrupt_lines.c core/scheduler/interrupt_lines.h core/bus/cpu_io.h core/cpu/65c816/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_MASTER_TIMING): tests/test_master_timing.c core/scheduler/master_timing.h core/scheduler/hv_timing.h core/scheduler/scheduler.h core/bus/cpu_io.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SCHEDULER): tests/test_scheduler.c core/scheduler/scheduler.h core/scheduler/5a22_timing.h core/cpu/65c816/cpu.h core/bus/bus.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_SNES_BUS): tests/test_snes_bus.c core/bus/snes_bus.h core/scheduler/cpu_timing.h core/scheduler/scheduler.h core/cpu/65c816/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CARTRIDGE): tests/test_cartridge.c core/cartridge/cartridge.h core/bus/snes_bus.h core/scheduler/cpu_timing.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CARTRIDGE_IMAGE): tests/test_cartridge_image.c core/cartridge/image.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CARTRIDGE_PREPARE): tests/test_cartridge_prepare.c core/cartridge/prepare.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PS2_DIAGNOSTIC): tests/test_ps2_diagnostic.c platform/ps2/diagnostic_suite.c platform/ps2/diagnostic_suite.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) tests/test_ps2_diagnostic.c platform/ps2/diagnostic_suite.c $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_MACHINE): tests/test_machine.c core/machine/machine.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PS2_ROM_FILE): tests/test_ps2_rom_file.c platform/ps2/rom_file.c platform/ps2/rom_file.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) tests/test_ps2_rom_file.c platform/ps2/rom_file.c $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PS2_VIDEO_FRAME): tests/test_ps2_video_frame.c platform/ps2/video_frame.c platform/ps2/video_frame.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) tests/test_ps2_video_frame.c platform/ps2/video_frame.c $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DMA_REGISTERS): tests/test_dma_registers.c core/dma/registers.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DMA_TRANSFER): tests/test_dma_transfer.c core/dma/transfer.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_HDMA): tests/test_hdma.c core/dma/hdma.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DMA_TIMING): tests/test_dma_timing.c core/scheduler/cpu_timing.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

check:
	$(PYTHON) tools/check_repository.py

test: all $(TEST) $(TEST_NOLOG) $(TEST_CPU_BUS) $(TEST_CPU_CYCLES) $(TEST_CPU_DIFFERENTIAL) $(TEST_CPU_IO) $(TEST_APU_IO_PORTS) $(TEST_PCM) $(TEST_DSP) $(TEST_APU_TIMING) $(TEST_SMP_TIMERS) $(TEST_SMP_MEMORY) $(TEST_SPC700_CPU) $(TEST_SPC700_ALU) $(TEST_SPC700_COMPLETE) $(TEST_DMA_REGISTERS) $(TEST_DMA_TRANSFER) $(TEST_HDMA) $(TEST_DMA_TIMING) $(TEST_PPU_COUNTERS) $(TEST_PPU_VRAM) $(TEST_PPU_CGRAM) $(TEST_PPU_OAM) $(TEST_PPU_TILE4) $(TEST_PPU_SPRITE_SCANLINE) $(TEST_PPU_BG2_SCANLINE) $(TEST_PPU_BG4_SCANLINE) $(TEST_PPU_BG_REGISTERS) $(TEST_PPU_MODE0_MIX) $(TEST_PPU_MODE1_MIX) $(TEST_PPU_WINDOW_MASK) $(TEST_PPU_COLOR_MATH) $(TEST_HV_TIMING) $(TEST_INTERRUPT_LINES) $(TEST_MASTER_TIMING) $(TEST_SCHEDULER) $(TEST_SNES_BUS) $(TEST_CARTRIDGE) $(TEST_CARTRIDGE_IMAGE) $(TEST_CARTRIDGE_PREPARE) $(TEST_PS2_DIAGNOSTIC) $(TEST_MACHINE) $(TEST_PS2_ROM_FILE) $(TEST_PS2_VIDEO_FRAME) check
	$(TEST)
	$(TEST_NOLOG)
	$(TEST_CPU_BUS)
	$(TEST_CPU_CYCLES)
	$(TEST_CPU_DIFFERENTIAL)
	$(TEST_CPU_IO)
	$(TEST_APU_IO_PORTS)
	$(TEST_PCM)
	$(TEST_DSP)
	$(TEST_APU_TIMING)
	$(TEST_SMP_TIMERS)
	$(TEST_SMP_MEMORY)
	$(TEST_SPC700_CPU)
	$(TEST_SPC700_ALU)
	$(TEST_SPC700_COMPLETE)
	$(TEST_DMA_REGISTERS)
	$(TEST_DMA_TRANSFER)
	$(TEST_HDMA)
	$(TEST_DMA_TIMING)
	$(TEST_PPU_COUNTERS)
	$(TEST_PPU_VRAM)
	$(TEST_PPU_CGRAM)
	$(TEST_PPU_OAM)
	$(TEST_PPU_TILE4)
	$(TEST_PPU_SPRITE_SCANLINE)
	$(TEST_PPU_BG2_SCANLINE)
	$(TEST_PPU_BG4_SCANLINE)
	$(TEST_PPU_BG_REGISTERS)
	$(TEST_PPU_MODE0_MIX)
	$(TEST_PPU_MODE1_MIX)
	$(TEST_PPU_WINDOW_MASK)
	$(TEST_PPU_COLOR_MATH)
	$(TEST_HV_TIMING)
	$(TEST_INTERRUPT_LINES)
	$(TEST_MASTER_TIMING)
	$(TEST_SCHEDULER)
	$(TEST_SNES_BUS)
	$(TEST_CARTRIDGE)
	$(TEST_CARTRIDGE_IMAGE)
	$(TEST_CARTRIDGE_PREPARE)
	$(TEST_PS2_DIAGNOSTIC)
	$(TEST_MACHINE)
	$(TEST_PS2_ROM_FILE)
	$(TEST_PS2_VIDEO_FRAME)
	$(PYTHON) tests/test_host.py $(HOST)

clean:
	$(RM) -r $(BUILD_DIR)

-include $(BUILD_DIR)/log.d $(BUILD_DIR)/bus.d $(BUILD_DIR)/cpu_io.d $(BUILD_DIR)/apu_io_ports.d $(BUILD_DIR)/pcm.d $(BUILD_DIR)/dsp.d $(BUILD_DIR)/smp_timers.d $(BUILD_DIR)/smp_memory.d $(BUILD_DIR)/spc700_cpu.d $(BUILD_DIR)/apu_timing.d $(BUILD_DIR)/dma_registers.d $(BUILD_DIR)/dma_transfer.d $(BUILD_DIR)/hdma.d \
	$(BUILD_DIR)/ppu_counters.d $(BUILD_DIR)/ppu_vram.d $(BUILD_DIR)/ppu_cgram.d $(BUILD_DIR)/ppu_oam.d $(BUILD_DIR)/ppu_tile4.d $(BUILD_DIR)/ppu_sprite_scanline.d $(BUILD_DIR)/ppu_bg2_scanline.d $(BUILD_DIR)/ppu_bg4_scanline.d $(BUILD_DIR)/ppu_bg_registers.d $(BUILD_DIR)/ppu_mode0_mix.d $(BUILD_DIR)/ppu_mode1_mix.d $(BUILD_DIR)/ppu_window_mask.d $(BUILD_DIR)/ppu_color_math.d \
	$(BUILD_DIR)/snes_bus.d \
	$(BUILD_DIR)/cpu.d $(BUILD_DIR)/scheduler.d \
	$(BUILD_DIR)/5a22_timing.d $(BUILD_DIR)/hv_timing.d \
	$(BUILD_DIR)/interrupt_lines.d \
	$(BUILD_DIR)/master_timing.d \
	$(BUILD_DIR)/cpu_timing.d \
	$(BUILD_DIR)/cartridge.d $(BUILD_DIR)/cartridge_image.d \
	$(BUILD_DIR)/cartridge_prepare.d $(BUILD_DIR)/machine.d
