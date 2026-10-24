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
TEST_CPU_IO = $(BUILD_DIR)/test-cpu-io
TEST_PPU_COUNTERS = $(BUILD_DIR)/test-ppu-counters
TEST_PPU_VRAM = $(BUILD_DIR)/test-ppu-vram
TEST_PPU_CGRAM = $(BUILD_DIR)/test-ppu-cgram
TEST_PPU_OAM = $(BUILD_DIR)/test-ppu-oam
TEST_PPU_TILE4 = $(BUILD_DIR)/test-ppu-tile4
TEST_PPU_SPRITE_SCANLINE = $(BUILD_DIR)/test-ppu-sprite-scanline
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
CORE_OBJS = $(BUILD_DIR)/log.o $(BUILD_DIR)/bus.o $(BUILD_DIR)/cpu_io.o $(BUILD_DIR)/ppu_counters.o $(BUILD_DIR)/ppu_vram.o $(BUILD_DIR)/ppu_cgram.o $(BUILD_DIR)/ppu_oam.o $(BUILD_DIR)/ppu_tile4.o $(BUILD_DIR)/ppu_sprite_scanline.o \
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

$(TEST_CPU_IO): tests/test_cpu_io.c core/bus/cpu_io.h $(LIB)
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

check:
	$(PYTHON) tools/check_repository.py

test: all $(TEST) $(TEST_NOLOG) $(TEST_CPU_BUS) $(TEST_CPU_CYCLES) $(TEST_CPU_IO) $(TEST_PPU_COUNTERS) $(TEST_PPU_VRAM) $(TEST_PPU_CGRAM) $(TEST_PPU_OAM) $(TEST_PPU_TILE4) $(TEST_PPU_SPRITE_SCANLINE) $(TEST_HV_TIMING) $(TEST_INTERRUPT_LINES) $(TEST_MASTER_TIMING) $(TEST_SCHEDULER) $(TEST_SNES_BUS) $(TEST_CARTRIDGE) $(TEST_CARTRIDGE_IMAGE) $(TEST_CARTRIDGE_PREPARE) $(TEST_PS2_DIAGNOSTIC) $(TEST_MACHINE) $(TEST_PS2_ROM_FILE) check
	$(TEST)
	$(TEST_NOLOG)
	$(TEST_CPU_BUS)
	$(TEST_CPU_CYCLES)
	$(TEST_CPU_IO)
	$(TEST_PPU_COUNTERS)
	$(TEST_PPU_VRAM)
	$(TEST_PPU_CGRAM)
	$(TEST_PPU_OAM)
	$(TEST_PPU_TILE4)
	$(TEST_PPU_SPRITE_SCANLINE)
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
	$(PYTHON) tests/test_host.py $(HOST)

clean:
	$(RM) -r $(BUILD_DIR)

-include $(BUILD_DIR)/log.d $(BUILD_DIR)/bus.d $(BUILD_DIR)/cpu_io.d \
	$(BUILD_DIR)/ppu_counters.d $(BUILD_DIR)/ppu_vram.d $(BUILD_DIR)/ppu_cgram.d $(BUILD_DIR)/ppu_oam.d $(BUILD_DIR)/ppu_tile4.d $(BUILD_DIR)/ppu_sprite_scanline.d \
	$(BUILD_DIR)/snes_bus.d \
	$(BUILD_DIR)/cpu.d $(BUILD_DIR)/scheduler.d \
	$(BUILD_DIR)/5a22_timing.d $(BUILD_DIR)/hv_timing.d \
	$(BUILD_DIR)/interrupt_lines.d \
	$(BUILD_DIR)/master_timing.d \
	$(BUILD_DIR)/cpu_timing.d \
	$(BUILD_DIR)/cartridge.d $(BUILD_DIR)/cartridge_image.d \
	$(BUILD_DIR)/cartridge_prepare.d $(BUILD_DIR)/machine.d
