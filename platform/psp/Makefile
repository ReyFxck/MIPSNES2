# SDK conventions verified against pspdev/pspsdk; see docs/psp.md.
TARGET = mipsnes2-psp
OBJS = main.o log.o
BUILD_PRX = 1
INCDIR = ../..
CFLAGS = -O2 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = MIPSNES2 Foundation - No emulation
PSPSDK := $(shell psp-config --pspsdk-path)
ifeq ($(strip $(PSPSDK)),)
$(error psp-config unavailable; see docs/psp.md)
endif

.PHONY: all
all: $(EXTRA_TARGETS)

log.o: ../../core/common/log.c ../../core/common/log.h
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -I$(PSPSDK)/include -c $< -o $@

main.o: main.c ../../core/common/log.h ../../core/common/project.h

include $(PSPSDK)/lib/build.mak
