/* MIPSNES2 SNES H/V timing foundation. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/hv_timing.h"

#include <limits.h>
#include <string.h>

static bool standard_is_valid(mipsnes2_hv_standard standard)
{
    return standard == MIPSNES2_HV_STANDARD_NTSC ||
           standard == MIPSNES2_HV_STANDARD_PAL;
}

static uint16_t previous_scanline_master_clocks(
    const mipsnes2_hv_timing *timing)
{
    uint16_t previous_v = timing->v_counter == 0U
                              ? (uint16_t)(mipsnes2_hv_frame_scanlines(timing) - 1U)
                              : (uint16_t)(timing->v_counter - 1U);
    bool previous_field = timing->field;

    if (timing->v_counter == 0U &&
        timing->h_master_clock >= MIPSNES2_HV_HBLANK_END_CLOCK) {
        previous_field = !previous_field;
    }
    if (timing->standard == MIPSNES2_HV_STANDARD_NTSC &&
        previous_field && previous_v == UINT16_C(240)) {
        return MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS;
    }
    return MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
}

static bool has_previous_scanline(const mipsnes2_hv_timing *timing)
{
    return timing->frame_count != 0U || timing->v_counter != 0U;
}

static bool h_timer_suppressed(const mipsnes2_hv_timing *timing,
                               uint16_t v_counter,
                               bool field,
                               uint16_t htime)
{
    bool short_line = timing->standard == MIPSNES2_HV_STANDARD_NTSC &&
                      field && v_counter == UINT16_C(240);
    bool last_line =
        v_counter + 1U == mipsnes2_hv_frame_scanlines(timing);

    return htime == UINT16_C(153) && (short_line || last_line);
}

static bool timer_match_clock(const mipsnes2_hv_timing *timing,
                              const mipsnes2_cpu_io *cpu_io,
                              uint16_t *match_clock)
{
    mipsnes2_cpu_io_irq_mode mode =
        mipsnes2_cpu_io_irq_mode_get(cpu_io);
    uint16_t current_length = mipsnes2_hv_scanline_master_clocks(timing);
    uint16_t frame_scanlines = mipsnes2_hv_frame_scanlines(timing);
    uint16_t htime = cpu_io->htime;
    uint16_t vtime = cpu_io->vtime;

    if (timing->timer_event_consumed ||
        mode == MIPSNES2_CPU_IO_IRQ_DISABLED) {
        return false;
    }
    if ((mode == MIPSNES2_CPU_IO_IRQ_H ||
         mode == MIPSNES2_CPU_IO_IRQ_HV) && htime > UINT16_C(339)) {
        return false;
    }
    if ((mode == MIPSNES2_CPU_IO_IRQ_V ||
         mode == MIPSNES2_CPU_IO_IRQ_HV) && vtime >= frame_scanlines) {
        return false;
    }

    if (mode == MIPSNES2_CPU_IO_IRQ_V ||
        (mode == MIPSNES2_CPU_IO_IRQ_HV && htime == 0U)) {
        if (timing->v_counter == vtime && has_previous_scanline(timing)) {
            *match_clock = (uint16_t)(UINT16_C(1374) -
                                      previous_scanline_master_clocks(timing));
            return true;
        }
        return false;
    }

    if (mode == MIPSNES2_CPU_IO_IRQ_H ||
        (mode == MIPSNES2_CPU_IO_IRQ_HV && timing->v_counter == vtime)) {
        uint16_t event_clock = (uint16_t)(UINT16_C(14) + htime * 4U);

        if (event_clock < current_length &&
            !h_timer_suppressed(timing, timing->v_counter,
                                timing->field, htime)) {
            *match_clock = event_clock;
            return true;
        }
    }

    if (has_previous_scanline(timing) &&
        (mode == MIPSNES2_CPU_IO_IRQ_H || mode == MIPSNES2_CPU_IO_IRQ_HV)) {
        uint16_t previous_length = previous_scanline_master_clocks(timing);
        uint16_t previous_v = timing->v_counter == 0U
                                  ? (uint16_t)(frame_scanlines - 1U)
                                  : (uint16_t)(timing->v_counter - 1U);
        bool previous_field = timing->field;
        uint16_t event_clock = (uint16_t)(UINT16_C(14) + htime * 4U);

        if (timing->v_counter == 0U &&
            timing->h_master_clock >= MIPSNES2_HV_HBLANK_END_CLOCK) {
            previous_field = !previous_field;
        }
        if (event_clock >= previous_length &&
            (mode == MIPSNES2_CPU_IO_IRQ_H || previous_v == vtime) &&
            !h_timer_suppressed(timing, previous_v, previous_field, htime)) {
            *match_clock = (uint16_t)(event_clock - previous_length);
            return true;
        }
    }
    return false;
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

bool mipsnes2_hv_clocks_until_frame_boundary(
    const mipsnes2_hv_timing *timing, uint64_t *clocks_out)
{
    uint16_t frame_scanlines;
    uint16_t line;
    uint64_t clocks = 0U;
    bool frame_field;

    if (timing == NULL || clocks_out == NULL || !timing->initialized ||
        !standard_is_valid(timing->standard)) {
        return false;
    }
    frame_scanlines = mipsnes2_hv_frame_scanlines(timing);
    if (timing->v_counter >= frame_scanlines ||
        timing->h_master_clock >=
            mipsnes2_hv_scanline_master_clocks(timing)) {
        return false;
    }

    frame_field = timing->field;
    if (timing->v_counter == 0U &&
        timing->h_master_clock < MIPSNES2_HV_HBLANK_END_CLOCK) {
        frame_field = !frame_field;
    }
    for (line = timing->v_counter; line < frame_scanlines; ++line) {
        uint16_t length = MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;

        if (timing->standard == MIPSNES2_HV_STANDARD_NTSC &&
            frame_field && line == UINT16_C(240)) {
            length = MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS;
        }
        clocks += line == timing->v_counter
                      ? (uint16_t)(length - timing->h_master_clock)
                      : length;
    }
    *clocks_out = clocks;
    return true;
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

bool mipsnes2_hv_ppu_horizontal(const mipsnes2_hv_timing *timing,
                                uint16_t *horizontal)
{
    uint16_t clocks;
    uint16_t length = mipsnes2_hv_scanline_master_clocks(timing);

    if (length == 0U || horizontal == NULL ||
        timing->v_counter >= mipsnes2_hv_frame_scanlines(timing) ||
        timing->h_master_clock >= length) {
        return false;
    }
    clocks = timing->h_master_clock;
    if (length == MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS) {
        /* Dots 323 and 327 last six clocks rather than four. */
        if (clocks >= UINT16_C(1312)) {
            clocks = (uint16_t)(clocks - 2U);
        }
        if (clocks >= UINT16_C(1294)) {
            clocks = (uint16_t)(clocks - 2U);
        }
    }
    *horizontal = (uint16_t)(clocks / 4U);
    return true;
}

static uint16_t next_horizontal_boundary(const mipsnes2_hv_timing *timing,
                                         const mipsnes2_cpu_io *cpu_io)
{
    uint16_t next = mipsnes2_hv_scanline_master_clocks(timing);
    uint16_t vblank_start = mipsnes2_hv_vblank_start_line(timing);
    uint16_t timer_clock;

    if (timing->v_counter == vblank_start &&
        timing->h_master_clock < MIPSNES2_HV_NMI_FLAG_CLOCK) {
        next = MIPSNES2_HV_NMI_FLAG_CLOCK;
    }
    if (timing->v_counter == vblank_start &&
        !timing->auto_joypad_start_consumed &&
        timing->h_master_clock < MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK &&
        MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK < next) {
        next = MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK;
    }
    if (timing->h_master_clock < MIPSNES2_HV_HBLANK_END_CLOCK &&
        MIPSNES2_HV_HBLANK_END_CLOCK < next) {
        next = MIPSNES2_HV_HBLANK_END_CLOCK;
    }
    if (timing->h_master_clock < MIPSNES2_HV_HBLANK_START_CLOCK &&
        MIPSNES2_HV_HBLANK_START_CLOCK < next) {
        next = MIPSNES2_HV_HBLANK_START_CLOCK;
    }
    if (timer_match_clock(timing, cpu_io, &timer_clock) &&
        timing->h_master_clock < timer_clock && timer_clock < next) {
        next = timer_clock;
    }
    return next;
}

static void process_timer_boundary(mipsnes2_hv_timing *timing,
                                   mipsnes2_cpu_io *cpu_io)
{
    uint16_t timer_clock;

    if (timer_match_clock(timing, cpu_io, &timer_clock) &&
        timing->h_master_clock == timer_clock) {
        mipsnes2_cpu_io_signal_timer_irq(cpu_io);
        timing->timer_event_consumed = true;
    }
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
    if (timing->h_master_clock == MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK &&
        timing->v_counter == mipsnes2_hv_vblank_start_line(timing) &&
        !timing->auto_joypad_start_consumed) {
        timing->auto_joypad_start_consumed = true;
        if (mipsnes2_cpu_io_begin_auto_joypad_read(cpu_io)) {
            timing->auto_joypad_clocks_remaining =
                MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS;
        }
    }
    process_timer_boundary(timing, cpu_io);
}

static void advance_auto_joypad(mipsnes2_hv_timing *timing,
                                mipsnes2_cpu_io *cpu_io,
                                uint16_t master_clocks)
{
    if (timing->auto_joypad_clocks_remaining == 0U) {
        return;
    }
    timing->auto_joypad_clocks_remaining =
        (uint16_t)(timing->auto_joypad_clocks_remaining - master_clocks);
    if (timing->auto_joypad_clocks_remaining == 0U) {
        (void)mipsnes2_cpu_io_complete_auto_joypad_read(cpu_io);
    }
}

static void advance_to_next_scanline(mipsnes2_hv_timing *timing,
                                     mipsnes2_cpu_io *cpu_io)
{
    uint16_t frame_scanlines = mipsnes2_hv_frame_scanlines(timing);

    timing->h_master_clock = 0U;
    timing->v_counter++;
    timing->timer_event_consumed = false;
    if (timing->v_counter == frame_scanlines) {
        timing->v_counter = 0U;
        timing->frame_count++;
        timing->auto_joypad_start_consumed = false;
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

    if (master_clocks != 0U) {
        process_timer_boundary(timing, cpu_io);
    }

    while (master_clocks != 0U) {
        uint16_t boundary = next_horizontal_boundary(timing, cpu_io);
        uint16_t distance = (uint16_t)(boundary - timing->h_master_clock);
        uint64_t step = master_clocks < distance ? master_clocks : distance;

        if (timing->auto_joypad_clocks_remaining != 0U &&
            timing->auto_joypad_clocks_remaining < step) {
            step = timing->auto_joypad_clocks_remaining;
        }

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
        advance_auto_joypad(timing, cpu_io, (uint16_t)step);

        if (timing->h_master_clock ==
            mipsnes2_hv_scanline_master_clocks(timing)) {
            advance_to_next_scanline(timing, cpu_io);
        } else {
            process_horizontal_boundary(timing, cpu_io);
        }
    }
    return MIPSNES2_HV_OK;
}
