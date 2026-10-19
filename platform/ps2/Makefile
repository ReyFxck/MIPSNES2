# SDK conventions verified against ps2dev/ps2sdk; see docs/ps2.md.
ifeq ($(strip $(PS2SDK)),)
$(error PS2SDK is unset; see docs/ps2.md)
endif
EE_BIN = mipsnes2-ps2.elf
EE_OBJS = main.o log.o bus.o cpu_io.o snes_bus.o cpu.o scheduler.o 5a22_timing.o \
	hv_timing.o cpu_timing.o cartridge.o cartridge_image.o cartridge_prepare.o
EE_LIBS = -lc
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

cpu_timing.o: ../../core/scheduler/cpu_timing.c ../../core/scheduler/cpu_timing.h ../../core/scheduler/scheduler.h ../../core/bus/snes_bus.h ../../core/cpu/65c816/cpu.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cartridge.o: ../../core/cartridge/cartridge.c ../../core/cartridge/cartridge.h ../../core/cartridge/interface.h ../../core/common/address.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cartridge_image.o: ../../core/cartridge/image.c ../../core/cartridge/image.h ../../core/cartridge/cartridge.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

cartridge_prepare.o: ../../core/cartridge/prepare.c ../../core/cartridge/prepare.h ../../core/cartridge/image.h ../../core/cartridge/cartridge.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

main.o: main.c ../../core/common/log.h ../../core/common/project.h

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
