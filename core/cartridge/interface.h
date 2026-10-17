/* MIPSNES2 cartridge-slot contract. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CARTRIDGE_INTERFACE_H
#define MIPSNES2_CARTRIDGE_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/common/address.h"

typedef bool (*mipsnes2_cartridge_read8)(void *context,
                                         mipsnes2_address address,
                                         uint8_t *value);
typedef bool (*mipsnes2_cartridge_write8)(void *context,
                                          mipsnes2_address address,
                                          uint8_t value);

typedef struct {
    void *context;
    mipsnes2_cartridge_read8 read8;
    mipsnes2_cartridge_write8 write8;
} mipsnes2_cartridge_interface;

#endif
