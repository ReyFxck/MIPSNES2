/* MIPSNES2 PPU counter-register foundation. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/counters.h"

#include <stddef.h>
#include <string.h>

void mipsnes2_ppu_counters_init(mipsnes2_ppu_counters *counters,
                                uint8_t version,
                                uint8_t initial_open_bus2)
{
    if (counters == NULL) {
        return;
    }
    (void)memset(counters, 0, sizeof(*counters));
    counters->version = version & UINT8_C(0x0f);
    counters->open_bus2 = initial_open_bus2;
    counters->initialized = true;
}

bool mipsnes2_ppu_counters_capture(mipsnes2_ppu_counters *counters,
                                   const mipsnes2_hv_timing *timing)
{
    uint16_t horizontal;

    if (counters == NULL || !counters->initialized ||
        !mipsnes2_hv_ppu_horizontal(timing, &horizontal)) {
        return false;
    }
    counters->horizontal = horizontal;
    counters->vertical = timing->v_counter;
    counters->latched = true;
    return true;
}

bool mipsnes2_ppu_counters_read(mipsnes2_ppu_counters *counters,
                                const mipsnes2_hv_timing *timing,
                                bool latch_enabled,
                                uint16_t offset,
                                uint8_t cpu_open_bus,
                                uint8_t *value)
{
    uint16_t horizontal;
    uint8_t result;

    if (counters == NULL || !counters->initialized || value == NULL ||
        !mipsnes2_hv_ppu_horizontal(timing, &horizontal)) {
        return false;
    }
    switch (offset) {
    case 0x2137:
        if (latch_enabled) {
            (void)mipsnes2_ppu_counters_capture(counters, timing);
        }
        *value = cpu_open_bus;
        return true;
    case 0x213c:
        result = counters->horizontal_high
                     ? (uint8_t)((counters->open_bus2 & UINT8_C(0xfe)) |
                                  ((counters->horizontal >> 8) & 1U))
                     : (uint8_t)counters->horizontal;
        counters->horizontal_high = !counters->horizontal_high;
        break;
    case 0x213d:
        result = counters->vertical_high
                     ? (uint8_t)((counters->open_bus2 & UINT8_C(0xfe)) |
                                  ((counters->vertical >> 8) & 1U))
                     : (uint8_t)counters->vertical;
        counters->vertical_high = !counters->vertical_high;
        break;
    case 0x213f:
        result = (uint8_t)((timing->field ? UINT8_C(0x80) : 0U) |
                           (counters->latched ? UINT8_C(0x40) : 0U) |
                           (counters->open_bus2 & UINT8_C(0x20)) |
                           (timing->standard == MIPSNES2_HV_STANDARD_PAL
                                ? UINT8_C(0x10) : 0U) |
                           counters->version);
        counters->horizontal_high = false;
        counters->vertical_high = false;
        if (latch_enabled) {
            counters->latched = false;
        }
        break;
    default:
        return false;
    }
    counters->open_bus2 = result;
    *value = result;
    return true;
}

bool mipsnes2_ppu_counters_write_ignored(uint16_t offset)
{
    return offset == UINT16_C(0x2137) || offset == UINT16_C(0x213c) ||
           offset == UINT16_C(0x213e) ||
           offset == UINT16_C(0x213d) || offset == UINT16_C(0x213f);
}
