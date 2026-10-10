/* MIPSNES2 logical bus foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_BUS_H
#define MIPSNES2_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/bus/interface.h"

#define MIPSNES2_BUS_MAX_REGIONS 16U

typedef enum {
    MIPSNES2_BUS_OK = 0,
    MIPSNES2_BUS_INVALID_ARGUMENT,
    MIPSNES2_BUS_OUT_OF_RANGE,
    MIPSNES2_BUS_OVERLAP,
    MIPSNES2_BUS_REGION_LIMIT
} mipsnes2_bus_result;

typedef enum {
    MIPSNES2_BUS_REGION_DIRECT = 0,
    MIPSNES2_BUS_REGION_MMIO
} mipsnes2_bus_region_kind;

typedef struct {
    mipsnes2_address first;
    mipsnes2_address last;
    mipsnes2_bus_region_kind kind;
    union {
        struct {
            uint8_t *bytes;
            bool writable;
        } direct;
        struct {
            void *context;
            mipsnes2_bus_read8 read8;
            mipsnes2_bus_write8 write8;
        } mmio;
    } target;
} mipsnes2_bus_region;

typedef struct {
    mipsnes2_bus_region regions[MIPSNES2_BUS_MAX_REGIONS];
    size_t region_count;
    uint8_t open_value;
} mipsnes2_bus;

void mipsnes2_bus_init(mipsnes2_bus *bus, uint8_t initial_open_value);
mipsnes2_bus_result mipsnes2_bus_map_direct(mipsnes2_bus *bus,
                                            mipsnes2_address first,
                                            size_t length,
                                            uint8_t *bytes,
                                            bool writable);
mipsnes2_bus_result mipsnes2_bus_map_mmio(mipsnes2_bus *bus,
                                          mipsnes2_address first,
                                          size_t length,
                                          mipsnes2_bus_read8 read8,
                                          mipsnes2_bus_write8 write8,
                                          void *context);
uint8_t mipsnes2_bus_read(mipsnes2_bus *bus, mipsnes2_address address);
void mipsnes2_bus_write(mipsnes2_bus *bus, mipsnes2_address address,
                        uint8_t value);
mipsnes2_bus_interface mipsnes2_bus_make_interface(mipsnes2_bus *bus);

#endif
