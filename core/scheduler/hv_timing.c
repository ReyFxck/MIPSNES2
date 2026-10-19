/* MIPSNES2 SNES H/V timing foundation. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/hv_timing.h"

#include <limits.h>
#include <string.h>

static bool standard_is_valid(mipsnes2_hv_standard standard)
{
    return standard == MIPSNES2_HV_STANDARD_NTSC ||
           standard == MIPSNES2_HV_STANDARD_PAL;
}

uint16_t mipsnes2_hv_frame_scanlines(const mipsnes2_hv_timing *timing)
{
    if (timing == NULL || !timing->initialized ||
        !standard_is_valid(timing->standard)) {
        return 0U;
    }
    return timing->standard == MIPSNES2_HV_STANDARD_NTSC
               ? MIPSNES2_HV_NTSC_SCANLINES
               : MIPSNES2_HV_PAL_SCANLINES;
}

uint16_t mipsnes2_hv_vblank_start_line(const mipsnes2_hv_timing *timing)
{
    if (timing == NULL || !timing->initialized ||
        !standard_is_valid(timing->standard)) {
        return 0U;
    }
    return timing->overscan ? UINT16_C(240) : UINT16_C(225);
}

uint16_t mipsnes2_hv_scanline_master_clocks(
    const mipsnes2_hv_timing *timing)
{
    if (timing == NULL || !timing->initialized ||
        !standard_is_valid(timing->standard)) {
        return 0U;
    }
    if (timing->standard == MIPSNES2_HV_STANDARD_NTSC &&
        timing->field && timing->v_counter == UINT16_C(240)) {
        return MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS;
    }
    return MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
}

mipsnes2_hv_result mipsnes2_hv_timing_init(
    mipsnes2_hv_timing *timing,
    mipsnes2_hv_standard standard,
    bool overscan,
    mipsnes2_cpu_io *cpu_io)
{
    if (timing == NULL || cpu_io == NULL || !standard_is_valid(standard)) {
        return MIPSNES2_HV_INVALID_ARGUMENT;
    }

    (void)memset(timing, 0, sizeof(*timing));
    timing->standard = standard;
    timing->overscan = overscan;
    timing->initialized = true;

    mipsnes2_cpu_io_set_vblank(cpu_io, false);
    mipsnes2_cpu_io_set_hblank(cpu_io, true);
    return MIPSNES2_HV_OK;
}

static uint16_t next_horizontal_boundary(const mipsnes2_hv_timing *timing)
{
    uint16_t next = mipsnes2_hv_scanline_master_clocks(timing);
    uint16_t vblank_start = mipsnes2_hv_vblank_start_line(timing);

    if (timing->v_counter == vblank_start &&
        timing->h_master_clock < MIPSNES2_HV_NMI_FLAG_CLOCK) {
        next = MIPSNES2_HV_NMI_FLAG_CLOCK;
    }
    if (timing->h_master_clock < MIPSNES2_HV_HBLANK_END_CLOCK &&
        MIPSNES2_HV_HBLANK_END_CLOCK < next) {
        next = MIPSNES2_HV_HBLANK_END_CLOCK;
    }
    if (timing->h_master_clock < MIPSNES2_HV_HBLANK_START_CLOCK &&
        MIPSNES2_HV_HBLANK_START_CLOCK < next) {
        next = MIPSNES2_HV_HBLANK_START_CLOCK;
    }
    return next;
}

static void process_horizontal_boundary(mipsnes2_hv_timing *timing,
                                        mipsnes2_cpu_io *cpu_io)
{
    if (timing->h_master_clock == MIPSNES2_HV_NMI_FLAG_CLOCK &&
        timing->v_counter == mipsnes2_hv_vblank_start_line(timing)) {
        mipsnes2_cpu_io_signal_nmi_flag(cpu_io);
    }
    if (timing->h_master_clock == MIPSNES2_HV_HBLANK_END_CLOCK) {
        mipsnes2_cpu_io_set_hblank(cpu_io, false);
        if (timing->v_counter == 0U) {
            timing->field = !timing->field;
        }
    }
    if (timing->h_master_clock == MIPSNES2_HV_HBLANK_START_CLOCK) {
        mipsnes2_cpu_io_set_hblank(cpu_io, true);
    }
}

static void advance_to_next_scanline(mipsnes2_hv_timing *timing,
                                     mipsnes2_cpu_io *cpu_io)
{
    uint16_t frame_scanlines = mipsnes2_hv_frame_scanlines(timing);

    timing->h_master_clock = 0U;
    timing->v_counter++;
    if (timing->v_counter == frame_scanlines) {
        timing->v_counter = 0U;
        timing->frame_count++;
        mipsnes2_cpu_io_set_vblank(cpu_io, false);
    } else if (timing->v_counter ==
               mipsnes2_hv_vblank_start_line(timing)) {
        mipsnes2_cpu_io_set_vblank(cpu_io, true);
    }
    mipsnes2_cpu_io_set_hblank(cpu_io, true);
}

mipsnes2_hv_result mipsnes2_hv_timing_advance(
    mipsnes2_hv_timing *timing,
    mipsnes2_cpu_io *cpu_io,
    uint64_t master_clocks)
{
    if (timing == NULL || cpu_io == NULL || !timing->initialized ||
        !standard_is_valid(timing->standard) ||
        timing->v_counter >= mipsnes2_hv_frame_scanlines(timing) ||
        timing->h_master_clock >=
            mipsnes2_hv_scanline_master_clocks(timing)) {
        return MIPSNES2_HV_INVALID_ARGUMENT;
    }
    if (master_clocks > UINT64_MAX - timing->elapsed_master_clocks) {
        return MIPSNES2_HV_TIME_OVERFLOW;
    }

    while (master_clocks != 0U) {
        uint16_t boundary = next_horizontal_boundary(timing);
        uint16_t distance = (uint16_t)(boundary - timing->h_master_clock);
        uint64_t step = master_clocks < distance ? master_clocks : distance;

        if (boundary == mipsnes2_hv_scanline_master_clocks(timing) &&
            master_clocks >= distance &&
            timing->v_counter + 1U ==
                mipsnes2_hv_frame_scanlines(timing) &&
            timing->frame_count == UINT64_MAX) {
            return MIPSNES2_HV_TIME_OVERFLOW;
        }

        timing->h_master_clock =
            (uint16_t)(timing->h_master_clock + (uint16_t)step);
        timing->elapsed_master_clocks += step;
        master_clocks -= step;

        if (timing->h_master_clock ==
            mipsnes2_hv_scanline_master_clocks(timing)) {
            advance_to_next_scanline(timing, cpu_io);
        } else {
            process_horizontal_boundary(timing, cpu_io);
        }
    }
    return MIPSNES2_HV_OK;
}
