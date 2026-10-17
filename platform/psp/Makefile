# SDK conventions verified against pspdev/pspsdk; see docs/psp.md.
TARGET = mipsnes2-psp
OBJS = main.o log.o bus.o snes_bus.o cpu.o scheduler.o 5a22_timing.o \
	cpu_timing.o
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

snes_bus.o: ../../core/bus/snes_bus.c ../../core/bus/snes_bus.h ../../core/bus/interface.h ../../core/common/address.h ../../core/scheduler/5a22_timing.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cpu.o: ../../core/cpu/65c816/cpu.c ../../core/cpu/65c816/cpu.h ../../core/bus/interface.h ../../core/common/address.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

scheduler.o: ../../core/scheduler/scheduler.c ../../core/scheduler/scheduler.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

5a22_timing.o: ../../core/scheduler/5a22_timing.c ../../core/scheduler/5a22_timing.h ../../core/common/address.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

cpu_timing.o: ../../core/scheduler/cpu_timing.c ../../core/scheduler/cpu_timing.h ../../core/scheduler/scheduler.h ../../core/bus/snes_bus.h ../../core/cpu/65c816/cpu.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

main.o: main.c ../../core/common/log.h ../../core/common/project.h

include $(PSPSDK)/lib/build.mak
