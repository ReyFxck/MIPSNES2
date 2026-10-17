/* MIPSNES2 Ricoh 5A22 timing classification. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_5A22_TIMING_H
#define MIPSNES2_5A22_TIMING_H

#include <stdbool.h>
#include <stdint.h>

#include "core/common/address.h"

enum {
    MIPSNES2_5A22_FAST_MASTER_CLOCKS = 6,
    MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS = 8,
    MIPSNES2_5A22_SLOW_MASTER_CLOCKS = 12,
    MIPSNES2_5A22_INTERNAL_MASTER_CLOCKS = 6
};

uint8_t mipsnes2_5a22_access_master_clocks(mipsnes2_address address,
                                            bool fastrom_enabled);

#endif
