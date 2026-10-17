/* MIPSNES2 Ricoh 5A22 timing classification. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/5a22_timing.h"

uint8_t mipsnes2_5a22_access_master_clocks(mipsnes2_address address,
                                            bool fastrom_enabled)
{
    uint8_t bank;
    uint16_t offset;

    address = mipsnes2_address24(address);
    bank = (uint8_t)(address >> 16);
    offset = (uint16_t)address;

    if (bank >= UINT8_C(0xc0)) {
        return fastrom_enabled ? MIPSNES2_5A22_FAST_MASTER_CLOCKS
                               : MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS;
    }
    if (bank >= UINT8_C(0x40) && bank < UINT8_C(0x80)) {
        return MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS;
    }

    if (offset < UINT16_C(0x2000)) {
        return MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS;
    }
    if (offset < UINT16_C(0x4000)) {
        return MIPSNES2_5A22_FAST_MASTER_CLOCKS;
    }
    if (offset < UINT16_C(0x4200)) {
        return MIPSNES2_5A22_SLOW_MASTER_CLOCKS;
    }
    if (offset < UINT16_C(0x6000)) {
        return MIPSNES2_5A22_FAST_MASTER_CLOCKS;
    }
    if (offset < UINT16_C(0x8000)) {
        return MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS;
    }

    if (bank >= UINT8_C(0x80)) {
        return fastrom_enabled ? MIPSNES2_5A22_FAST_MASTER_CLOCKS
                               : MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS;
    }
    return MIPSNES2_5A22_MEDIUM_MASTER_CLOCKS;
}
