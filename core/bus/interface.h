/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_BUS_INTERFACE_H
#define MIPSNES2_BUS_INTERFACE_H
#include "core/common/address.h"
/* CPU-facing byte-access contract. The caller owns context. Implementations
 * receive logical SNES addresses; native pointers never cross this boundary.
 * This interface is suitable for the reference core and test fixtures. It does
 * not freeze the eventual measured page/block fast path. */
typedef uint8_t (*mipsnes2_bus_read8)(void *context, mipsnes2_address address);
typedef void (*mipsnes2_bus_write8)(void *context, mipsnes2_address address,
                                 uint8_t value);
typedef struct {
    void *context;
    mipsnes2_bus_read8 read8;
    mipsnes2_bus_write8 write8;
} mipsnes2_bus_interface;
#endif
