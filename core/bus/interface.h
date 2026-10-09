/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_BUS_INTERFACE_H
#define MIPSNES2_BUS_INTERFACE_H
#include "core/common/address.h"
/* Provisional byte-access contract for future fixtures, NOT a working bus.
 * Caller owns context and normalizes addresses. Timing/side effects are not
 * implemented. The final hot-path dispatch design requires target profiling. */
typedef uint8_t (*mipsnes2_bus_read8)(void *context, mipsnes2_address address);
typedef void (*mipsnes2_bus_write8)(void *context, mipsnes2_address address,
                                 uint8_t value);
typedef struct {
    void *context;
    mipsnes2_bus_read8 read8;
    mipsnes2_bus_write8 write8;
} mipsnes2_bus_interface;
#endif
