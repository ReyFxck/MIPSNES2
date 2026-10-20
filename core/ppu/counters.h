/* MIPSNES2 PPU counter-register foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_COUNTERS_H
#define MIPSNES2_PPU_COUNTERS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/scheduler/hv_timing.h"

typedef struct {
    uint16_t horizontal;
    uint16_t vertical;
    uint8_t open_bus2;
    uint8_t version;
    bool horizontal_high;
    bool vertical_high;
    bool latched;
    bool initialized;
} mipsnes2_ppu_counters;

/* Deterministic storage initialization, not signal-level hardware RESET. */
void mipsnes2_ppu_counters_init(mipsnes2_ppu_counters *counters,
                                uint8_t version,
                                uint8_t initial_open_bus2);
bool mipsnes2_ppu_counters_capture(mipsnes2_ppu_counters *counters,
                                   const mipsnes2_hv_timing *timing);
bool mipsnes2_ppu_counters_read(mipsnes2_ppu_counters *counters,
                                const mipsnes2_hv_timing *timing,
                                bool latch_enabled,
                                uint16_t offset,
                                uint8_t cpu_open_bus,
                                uint8_t *value);
bool mipsnes2_ppu_counters_write_ignored(uint16_t offset);

#endif
