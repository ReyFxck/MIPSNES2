# SDK conventions verified against ps2dev/ps2sdk; see docs/ps2.md.
ifeq ($(strip $(PS2SDK)),)
$(error PS2SDK is unset; see docs/ps2.md)
endif
EE_BIN = mipsnes2-ps2.elf
EE_OBJS = main.o log.o
EE_LIBS = -lc
EE_INCS = -I../..
EE_CFLAGS = -std=c99 -Wextra -Werror

.PHONY: all clean
all: $(EE_BIN)

log.o: ../../core/common/log.c ../../core/common/log.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

main.o: main.c ../../core/common/log.h ../../core/common/project.h

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
