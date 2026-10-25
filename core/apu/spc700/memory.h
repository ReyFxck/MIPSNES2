/* MIPSNES2 S-SMP local memory map. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_SMP_MEMORY_H
#define MIPSNES2_SMP_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/apu/io_ports.h"

enum {
    MIPSNES2_SMP_RAM_SIZE = 65536,
    MIPSNES2_SMP_IPL_SIZE = 64
};

/* All storage is borrowed. The optional IPL image is supplied by the caller;
 * MIPSNES2 does not contain or manufacture firmware bytes. Keep this object at
 * a stable address while clients use it. */
typedef struct {
    uint8_t *ram;
    const uint8_t *ipl;
    mipsnes2_apu_io_ports *ports;
    bool ipl_enabled;
    bool initialized;
} mipsnes2_smp_memory;

bool mipsnes2_smp_memory_init(mipsnes2_smp_memory *memory,
                              uint8_t *ram, size_t ram_size,
                              mipsnes2_apu_io_ports *ports,
                              const uint8_t *ipl, size_t ipl_size);
/* Resets the register map, retaining the caller's ARAM and optional IPL. */
bool mipsnes2_smp_memory_reset(mipsnes2_smp_memory *memory);

/* False means invalid state, unavailable IPL, or an unimplemented MMIO
 * register. In these cases no output byte or modeled state is changed. */
bool mipsnes2_smp_memory_read(mipsnes2_smp_memory *memory,
                              uint16_t address, uint8_t *value);
bool mipsnes2_smp_memory_write(mipsnes2_smp_memory *memory,
                               uint16_t address, uint8_t value);

#endif
