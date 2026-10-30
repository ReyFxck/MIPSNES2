/* MIPSNES2 SNES H/V timing foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_HV_TIMING_H
#define MIPSNES2_HV_TIMING_H

#include <stdbool.h>
#include <stdint.h>

#include "core/bus/cpu_io.h"

enum {
    MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS = 1364,
    MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS = 1360,
    MIPSNES2_HV_NTSC_SCANLINES = 262,
    MIPSNES2_HV_PAL_SCANLINES = 312,
    MIPSNES2_HV_HBLANK_END_CLOCK = 4,
    MIPSNES2_HV_NMI_FLAG_CLOCK = 2,
    MIPSNES2_HV_HBLANK_START_CLOCK = 1096,
    MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK = 298,
    MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS = 4224
};

typedef enum {
    MIPSNES2_HV_STANDARD_NTSC = 0,
    MIPSNES2_HV_STANDARD_PAL = 1
} mipsnes2_hv_standard;

typedef enum {
    MIPSNES2_HV_OK = 0,
    MIPSNES2_HV_INVALID_ARGUMENT,
    MIPSNES2_HV_TIME_OVERFLOW
} mipsnes2_hv_result;

typedef struct {
    uint64_t elapsed_master_clocks;
    uint64_t frame_count;
    uint16_t h_master_clock;
    uint16_t v_counter;
    uint16_t auto_joypad_clocks_remaining;
    mipsnes2_hv_standard standard;
    bool overscan;
    bool field;
    bool timer_event_consumed;
    bool auto_joypad_start_consumed;
    bool initialized;
} mipsnes2_hv_timing;

mipsnes2_hv_result mipsnes2_hv_timing_init(
    mipsnes2_hv_timing *timing,
    mipsnes2_hv_standard standard,
    bool overscan,
    mipsnes2_cpu_io *cpu_io);
mipsnes2_hv_result mipsnes2_hv_timing_advance(
    mipsnes2_hv_timing *timing,
    mipsnes2_cpu_io *cpu_io,
    uint64_t master_clocks);
uint16_t mipsnes2_hv_scanline_master_clocks(
    const mipsnes2_hv_timing *timing);
uint16_t mipsnes2_hv_frame_scanlines(const mipsnes2_hv_timing *timing);
/* Exact distance from the current H/V state to the next frame boundary,
 * including the NTSC short scanline selected by the active field. On failure
 * clocks_out is unchanged.
 */
bool mipsnes2_hv_clocks_until_frame_boundary(
    const mipsnes2_hv_timing *timing, uint64_t *clocks_out);
uint16_t mipsnes2_hv_vblank_start_line(
    const mipsnes2_hv_timing *timing);
bool mipsnes2_hv_ppu_horizontal(const mipsnes2_hv_timing *timing,
                                uint16_t *horizontal);

#endif
