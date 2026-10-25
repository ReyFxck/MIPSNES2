/* MIPSNES2 S-CPU/S-SMP communication ports. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_APU_IO_PORTS_H
#define MIPSNES2_APU_IO_PORTS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/bus/interface.h"

enum { MIPSNES2_APU_IO_PORT_COUNT = 4 };

typedef struct {
    uint8_t cpu_to_apu[MIPSNES2_APU_IO_PORT_COUNT];
    uint8_t apu_to_cpu[MIPSNES2_APU_IO_PORT_COUNT];
    bool initialized;
} mipsnes2_apu_io_ports;

void mipsnes2_apu_io_ports_init(mipsnes2_apu_io_ports *ports);

/* S-CPU view at $2140-$217F. The four ports mirror every four addresses. */
bool mipsnes2_apu_io_ports_cpu_read(const mipsnes2_apu_io_ports *ports,
                                    uint16_t address, uint8_t *value);
bool mipsnes2_apu_io_ports_cpu_write(mipsnes2_apu_io_ports *ports,
                                     uint16_t address, uint8_t value);

/* S-SMP view at its $F4-$F7 register indices, represented as 0..3 here. */
bool mipsnes2_apu_io_ports_smp_read(const mipsnes2_apu_io_ports *ports,
                                    unsigned port, uint8_t *value);
bool mipsnes2_apu_io_ports_smp_write(mipsnes2_apu_io_ports *ports,
                                     unsigned port, uint8_t value);
/* $F1 bits 4/5 clear only the S-CPU-to-S-SMP input latch pairs. */
bool mipsnes2_apu_io_ports_smp_clear_inputs(mipsnes2_apu_io_ports *ports,
                                            bool ports_01, bool ports_23);

/* Adapter for the existing S-CPU bus route. */
mipsnes2_bus_interface mipsnes2_apu_io_ports_make_cpu_interface(
    mipsnes2_apu_io_ports *ports);

#endif
