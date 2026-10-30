/* MIPSNES2 SNES H/V timing tests. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/cpu_io.h"
#include "core/scheduler/hv_timing.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static int advance_to_line_start(mipsnes2_hv_timing *timing,
                                 mipsnes2_cpu_io *cpu_io,
                                 uint16_t target)
{
    while (timing->v_counter != target || timing->h_master_clock != 0U) {
        uint16_t line_clocks = mipsnes2_hv_scanline_master_clocks(timing);
        uint16_t remaining =
            (uint16_t)(line_clocks - timing->h_master_clock);

        if (mipsnes2_hv_timing_advance(timing, cpu_io, remaining) !=
            MIPSNES2_HV_OK) {
            return 1;
        }
    }
    return 0;
}

static int set_htime(mipsnes2_cpu_io *cpu_io, uint16_t htime)
{
    if (!mipsnes2_cpu_io_write(cpu_io, UINT16_C(0x4207),
                               (uint8_t)htime) ||
        !mipsnes2_cpu_io_write(cpu_io, UINT16_C(0x4208),
                               (uint8_t)(htime >> 8))) {
        return 1;
    }
    return 0;
}

static int set_vtime(mipsnes2_cpu_io *cpu_io, uint16_t vtime)
{
    if (!mipsnes2_cpu_io_write(cpu_io, UINT16_C(0x4209),
                               (uint8_t)vtime) ||
        !mipsnes2_cpu_io_write(cpu_io, UINT16_C(0x420a),
                               (uint8_t)(vtime >> 8))) {
        return 1;
    }
    return 0;
}

static int acknowledge_timer_irq(mipsnes2_cpu_io *cpu_io)
{
    uint8_t value;

    if (!mipsnes2_cpu_io_read(cpu_io, UINT16_C(0x4211), 0U, &value)) {
        return 1;
    }
    return 0;
}

static int test_initialization_and_validation(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_hv_timing before;

    (void)memset(&timing, 0xa5, sizeof(timing));
    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    before = timing;
    CHECK(mipsnes2_hv_timing_init(NULL, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) ==
          MIPSNES2_HV_INVALID_ARGUMENT);
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, NULL) ==
          MIPSNES2_HV_INVALID_ARGUMENT);
    CHECK(mipsnes2_hv_timing_init(&timing, (mipsnes2_hv_standard)2,
                                  false, &cpu_io) ==
          MIPSNES2_HV_INVALID_ARGUMENT);
    CHECK(memcmp(&timing, &before, sizeof(timing)) == 0);

    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(timing.initialized && timing.standard == MIPSNES2_HV_STANDARD_NTSC);
    CHECK(!timing.overscan && !timing.field && !timing.timer_event_consumed);
    CHECK(timing.elapsed_master_clocks == 0U && timing.frame_count == 0U);
    CHECK(timing.h_master_clock == 0U && timing.v_counter == 0U);
    CHECK(!cpu_io.vblank && cpu_io.hblank && !cpu_io.nmi_flag);
    CHECK(mipsnes2_hv_frame_scanlines(&timing) ==
          MIPSNES2_HV_NTSC_SCANLINES);
    CHECK(mipsnes2_hv_vblank_start_line(&timing) == UINT16_C(225));
    CHECK(mipsnes2_hv_scanline_master_clocks(&timing) ==
          MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS);
    CHECK(mipsnes2_hv_frame_scanlines(NULL) == 0U);
    CHECK(mipsnes2_hv_vblank_start_line(NULL) == 0U);
    CHECK(mipsnes2_hv_scanline_master_clocks(NULL) == 0U);
    return 0;
}

static int test_horizontal_blank_and_line_boundary(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(3)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(3) && cpu_io.hblank);
    CHECK(!timing.field);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == MIPSNES2_HV_HBLANK_END_CLOCK);
    CHECK(!cpu_io.hblank && timing.field);

    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_HBLANK_START_CLOCK -
                  MIPSNES2_HV_HBLANK_END_CLOCK - 1U) == MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock ==
          MIPSNES2_HV_HBLANK_START_CLOCK - 1U);
    CHECK(!cpu_io.hblank);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == MIPSNES2_HV_HBLANK_START_CLOCK);
    CHECK(cpu_io.hblank);

    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                  MIPSNES2_HV_HBLANK_START_CLOCK) == MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == 0U && timing.v_counter == UINT16_C(1));
    CHECK(cpu_io.hblank && timing.frame_count == 0U);
    CHECK(timing.elapsed_master_clocks ==
          MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS);
    return 0;
}

static int test_vblank_and_delayed_nmi_flag(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;
    uint8_t value;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x80)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(225)) == 0);
    CHECK(cpu_io.vblank && cpu_io.hblank);
    CHECK(!cpu_io.nmi_flag && !mipsnes2_cpu_io_take_nmi_edge(&cpu_io));

    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(1));
    CHECK(!cpu_io.nmi_flag);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == MIPSNES2_HV_NMI_FLAG_CLOCK);
    CHECK(cpu_io.nmi_flag && mipsnes2_cpu_io_take_nmi_edge(&cpu_io));
    CHECK(mipsnes2_cpu_io_read(&cpu_io, UINT16_C(0x4210),
                                UINT8_C(0x70), &value));
    CHECK(value == UINT8_C(0xf2) && !cpu_io.nmi_flag);
    CHECK(cpu_io.vblank);

    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(2)) ==
          MIPSNES2_HV_OK);
    CHECK(!cpu_io.hblank && !cpu_io.nmi_flag);
    return 0;
}

static int test_overscan_vblank_and_ntsc_short_line(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  true, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_vblank_start_line(&timing) == UINT16_C(240));
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(225)) == 0);
    CHECK(!cpu_io.vblank && timing.field);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(240)) == 0);
    CHECK(cpu_io.vblank);
    CHECK(mipsnes2_hv_scanline_master_clocks(&timing) ==
          MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(2)) ==
          MIPSNES2_HV_OK);
    CHECK(cpu_io.nmi_flag);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS - 2U) ==
          MIPSNES2_HV_OK);
    CHECK(timing.v_counter == UINT16_C(241) && timing.h_master_clock == 0U);
    return 0;
}

static int test_ntsc_frame_lengths_and_field_toggle(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;
    const uint64_t short_field_frame = UINT64_C(357364);
    const uint64_t normal_field_frame = UINT64_C(357368);

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                     short_field_frame) == MIPSNES2_HV_OK);
    CHECK(timing.frame_count == UINT64_C(1));
    CHECK(timing.v_counter == 0U && timing.h_master_clock == 0U);
    CHECK(timing.field && !cpu_io.vblank && cpu_io.hblank);

    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                     normal_field_frame) == MIPSNES2_HV_OK);
    CHECK(timing.frame_count == UINT64_C(2));
    CHECK(timing.v_counter == 0U && timing.h_master_clock == 0U);
    CHECK(!timing.field);
    CHECK(timing.elapsed_master_clocks == UINT64_C(714732));
    return 0;
}

static int test_frame_boundary_distance(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;
    uint64_t clocks = UINT64_C(0xfeedface);

    CHECK(!mipsnes2_hv_clocks_until_frame_boundary(NULL, &clocks));
    CHECK(clocks == UINT64_C(0xfeedface));
    (void)memset(&timing, 0, sizeof(timing));
    CHECK(!mipsnes2_hv_clocks_until_frame_boundary(&timing, &clocks));
    CHECK(!mipsnes2_hv_clocks_until_frame_boundary(&timing, NULL));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_clocks_until_frame_boundary(&timing, &clocks));
    CHECK(clocks == UINT64_C(357364));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1000)) ==
          MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_clocks_until_frame_boundary(&timing, &clocks));
    CHECK(clocks == UINT64_C(356364));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, clocks) ==
          MIPSNES2_HV_OK);
    CHECK(timing.frame_count == UINT64_C(1));
    CHECK(timing.v_counter == 0U && timing.h_master_clock == 0U);
    CHECK(mipsnes2_hv_clocks_until_frame_boundary(&timing, &clocks));
    CHECK(clocks == UINT64_C(357368));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_PAL,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_clocks_until_frame_boundary(&timing, &clocks));
    CHECK(clocks == UINT64_C(425568));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                     UINT64_C(425567)) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_clocks_until_frame_boundary(&timing, &clocks));
    CHECK(clocks == UINT64_C(1));
    return 0;
}

static int test_next_boundary_distance(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;
    uint64_t clocks = UINT64_C(0xa5a5a5a5);

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(!mipsnes2_hv_clocks_until_next_boundary(
        NULL, &cpu_io, &clocks));
    CHECK(clocks == UINT64_C(0xa5a5a5a5));
    (void)memset(&timing, 0, sizeof(timing));
    CHECK(!mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, &clocks));
    CHECK(!mipsnes2_hv_clocks_until_next_boundary(
        &timing, NULL, &clocks));
    CHECK(!mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, NULL));
    CHECK(clocks == UINT64_C(0xa5a5a5a5));

    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, &clocks));
    CHECK(clocks == MIPSNES2_HV_HBLANK_END_CLOCK);
    CHECK(mipsnes2_hv_timing_advance(
        &timing, &cpu_io, clocks) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, &clocks));
    CHECK(clocks == MIPSNES2_HV_HBLANK_START_CLOCK -
                    MIPSNES2_HV_HBLANK_END_CLOCK);

    CHECK(set_htime(&cpu_io, UINT16_C(10)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(0x10)));
    CHECK(mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, &clocks));
    CHECK(clocks == UINT64_C(50));
    CHECK(mipsnes2_hv_timing_advance(
        &timing, &cpu_io, clocks) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(0x80)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(225)) == 0);
    CHECK(mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, &clocks));
    CHECK(clocks == MIPSNES2_HV_NMI_FLAG_CLOCK);

    timing.h_master_clock = UINT16_C(100);
    timing.auto_joypad_clocks_remaining = UINT16_C(7);
    CHECK(mipsnes2_hv_clocks_until_next_boundary(
        &timing, &cpu_io, &clocks));
    CHECK(clocks == UINT64_C(7));
    return 0;
}

static int test_h_irq_normal_and_late_spill(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(10)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(53)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(54));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(acknowledge_timer_irq(&cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(338)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + 1U) == MIPSNES2_HV_OK);
    CHECK(timing.v_counter == UINT16_C(1) &&
          timing.h_master_clock == UINT16_C(1));
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(2));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(339)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + 5U) == MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(6));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));
    return 0;
}

static int test_all_h_irq_values(void)
{
    uint16_t htime;

    for (htime = 0U; htime <= UINT16_C(339); ++htime) {
        mipsnes2_hv_timing timing;
        mipsnes2_cpu_io cpu_io;
        uint64_t event_clock = UINT64_C(14) + (uint64_t)htime * UINT64_C(4);

        mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
        CHECK(set_htime(&cpu_io, htime) == 0);
        CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                    UINT8_C(0x10)));
        CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                      false, &cpu_io) == MIPSNES2_HV_OK);
        CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                         event_clock - 1U) ==
              MIPSNES2_HV_OK);
        CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
        CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
              MIPSNES2_HV_OK);
        CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));
    }
    return 0;
}

static int test_v_and_hv_irq_positions(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_vtime(&cpu_io, UINT16_C(1)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x20)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + 9U) == MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.v_counter == UINT16_C(1) &&
          timing.h_master_clock == UINT16_C(10));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, 0U) == 0);
    CHECK(set_vtime(&cpu_io, UINT16_C(1)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x30)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + 10U) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(5)) == 0);
    CHECK(set_vtime(&cpu_io, UINT16_C(1)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x30)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + 33U) == MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(34));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));
    return 0;
}

static int test_v_irq_after_short_line_and_suppressed_h153(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_vtime(&cpu_io, UINT16_C(241)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x20)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(241)) == 0);
    CHECK(timing.field && !mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(13)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock == UINT16_C(14));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(153)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(240)) == 0);
    CHECK(acknowledge_timer_irq(&cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(626)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS - 626U) ==
          MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(626)) ==
          MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(261)) == 0);
    CHECK(acknowledge_timer_irq(&cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(626)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(153)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_PAL,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(311)) == 0);
    CHECK(acknowledge_timer_irq(&cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(626)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    return 0;
}

static int test_irq_range_and_enable_boundary(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(340)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS * 2U) == MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_htime(&cpu_io, UINT16_C(10)) == 0);
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(54)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x10)));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_vtime(&cpu_io, 0U) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x20)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                     UINT64_C(357364) + 9U) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.v_counter == 0U && timing.h_master_clock == UINT16_C(10));
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_vtime(&cpu_io, MIPSNES2_HV_NTSC_SCANLINES) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x20)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                     UINT64_C(714732)) == MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_vtime(&cpu_io, UINT16_C(311)) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x20)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_PAL,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(311)) == 0);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(9)) ==
          MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_irq_asserted(&cpu_io));

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(set_vtime(&cpu_io, MIPSNES2_HV_PAL_SCANLINES) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x20)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_PAL,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io,
                                     UINT64_C(851136)) == MIPSNES2_HV_OK);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&cpu_io));
    return 0;
}

static uint16_t read_joypad_result(mipsnes2_cpu_io *cpu_io, uint8_t index)
{
    uint16_t offset = (uint16_t)(UINT16_C(0x4218) + index * 2U);
    uint8_t low = 0U;
    uint8_t high = 0U;

    (void)mipsnes2_cpu_io_read(cpu_io, offset, 0U, &low);
    (void)mipsnes2_cpu_io_read(cpu_io, (uint16_t)(offset + 1U), 0U, &high);
    return (uint16_t)((uint16_t)low | ((uint16_t)high << 8));
}

static int test_auto_joypad_timing_and_snapshot(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;
    uint8_t value;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io, 0U, UINT16_C(0x9080)));
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io, 1U, UINT16_C(0x7060)));
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io, 2U, UINT16_C(0x5040)));
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io, 3U, UINT16_C(0x3020)));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(1)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(225)) == 0);
    CHECK(cpu_io.vblank && !cpu_io.auto_joypad_busy);
    CHECK(!timing.auto_joypad_start_consumed);

    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK - 1U) ==
          MIPSNES2_HV_OK);
    CHECK(!cpu_io.auto_joypad_busy);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(timing.h_master_clock ==
          MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK);
    CHECK(timing.auto_joypad_start_consumed);
    CHECK(timing.auto_joypad_clocks_remaining ==
          MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS);
    CHECK(cpu_io.auto_joypad_busy);
    CHECK(mipsnes2_cpu_io_read(&cpu_io, UINT16_C(0x4212), 0U, &value));
    CHECK(value == UINT8_C(0x81));

    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io, 0U, UINT16_C(0xffff)));
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS - 1U) ==
          MIPSNES2_HV_OK);
    CHECK(cpu_io.auto_joypad_busy);
    CHECK(timing.auto_joypad_clocks_remaining == UINT16_C(1));
    CHECK(read_joypad_result(&cpu_io, 0U) == 0U);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(1)) ==
          MIPSNES2_HV_OK);
    CHECK(!cpu_io.auto_joypad_busy);
    CHECK(timing.auto_joypad_clocks_remaining == 0U);
    CHECK(timing.v_counter == UINT16_C(228));
    CHECK(timing.h_master_clock == UINT16_C(430));
    CHECK(read_joypad_result(&cpu_io, 0U) == UINT16_C(0x9080));
    CHECK(read_joypad_result(&cpu_io, 1U) == UINT16_C(0x7060));
    CHECK(read_joypad_result(&cpu_io, 2U) == UINT16_C(0x5040));
    CHECK(read_joypad_result(&cpu_io, 3U) == UINT16_C(0x3020));
    return 0;
}

static int test_auto_joypad_enable_phase_and_overscan(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io cpu_io;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(225)) == 0);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK) == MIPSNES2_HV_OK);
    CHECK(timing.auto_joypad_start_consumed);
    CHECK(!cpu_io.auto_joypad_busy);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(1)));
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, UINT64_C(5000)) ==
          MIPSNES2_HV_OK);
    CHECK(!cpu_io.auto_joypad_busy);

    CHECK(advance_to_line_start(&timing, &cpu_io, 0U) == 0);
    CHECK(!timing.auto_joypad_start_consumed);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(225)) == 0);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK) == MIPSNES2_HV_OK);
    CHECK(cpu_io.auto_joypad_busy);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), 0U));
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS) == MIPSNES2_HV_OK);
    CHECK(!cpu_io.auto_joypad_busy);

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(1)));
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  true, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(advance_to_line_start(&timing, &cpu_io, UINT16_C(240)) == 0);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK) == MIPSNES2_HV_OK);
    CHECK(timing.v_counter == UINT16_C(240));
    CHECK(cpu_io.auto_joypad_busy);
    CHECK(mipsnes2_hv_timing_advance(
              &timing, &cpu_io,
              MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS) == MIPSNES2_HV_OK);
    CHECK(!cpu_io.auto_joypad_busy);
    CHECK(timing.v_counter == UINT16_C(243));
    CHECK(timing.h_master_clock == UINT16_C(434));
    return 0;
}

static int test_pal_frame_and_chunk_determinism(void)
{
    mipsnes2_hv_timing whole;
    mipsnes2_hv_timing chunked;
    mipsnes2_cpu_io whole_io;
    mipsnes2_cpu_io chunked_io;
    uint64_t remaining = UINT64_C(500000);
    uint64_t chunk = UINT64_C(1);

    mipsnes2_cpu_io_init(&whole_io, UINT8_C(2));
    mipsnes2_cpu_io_init(&chunked_io, UINT8_C(2));
    CHECK(set_htime(&whole_io, UINT16_C(10)) == 0);
    CHECK(set_htime(&chunked_io, UINT16_C(10)) == 0);
    CHECK(mipsnes2_cpu_io_write(&whole_io, UINT16_C(0x4200),
                                UINT8_C(0x91)));
    CHECK(mipsnes2_cpu_io_write(&chunked_io, UINT16_C(0x4200),
                                UINT8_C(0x91)));
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &whole_io, 0U, UINT16_C(0xa5a5)));
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &chunked_io, 0U, UINT16_C(0xa5a5)));
    CHECK(mipsnes2_hv_timing_init(&whole, MIPSNES2_HV_STANDARD_PAL,
                                  false, &whole_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_init(&chunked, MIPSNES2_HV_STANDARD_PAL,
                                  false, &chunked_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_frame_scanlines(&whole) ==
          MIPSNES2_HV_PAL_SCANLINES);
    CHECK(mipsnes2_hv_timing_advance(&whole, &whole_io,
                                     UINT64_C(500000)) == MIPSNES2_HV_OK);

    while (remaining != 0U) {
        uint64_t step = chunk < remaining ? chunk : remaining;
        CHECK(mipsnes2_hv_timing_advance(&chunked, &chunked_io, step) ==
              MIPSNES2_HV_OK);
        remaining -= step;
        chunk = (chunk * UINT64_C(7)) % UINT64_C(4093) + UINT64_C(1);
    }
    CHECK(memcmp(&whole, &chunked, sizeof(whole)) == 0);
    CHECK(memcmp(&whole_io, &chunked_io, sizeof(whole_io)) == 0);

    mipsnes2_cpu_io_init(&whole_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(&whole, MIPSNES2_HV_STANDARD_PAL,
                                  false, &whole_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&whole, &whole_io,
                                     UINT64_C(425568)) == MIPSNES2_HV_OK);
    CHECK(whole.frame_count == UINT64_C(1));
    CHECK(whole.v_counter == 0U && whole.h_master_clock == 0U);
    CHECK(whole.field && !whole_io.vblank && whole_io.hblank);
    return 0;
}

static int test_advance_errors(void)
{
    mipsnes2_hv_timing timing;
    mipsnes2_hv_timing before;
    mipsnes2_cpu_io cpu_io;

    (void)memset(&timing, 0, sizeof(timing));
    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_advance(NULL, &cpu_io, 1U) ==
          MIPSNES2_HV_INVALID_ARGUMENT);
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, 1U) ==
          MIPSNES2_HV_INVALID_ARGUMENT);
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_hv_timing_advance(&timing, NULL, 1U) ==
          MIPSNES2_HV_INVALID_ARGUMENT);
    timing.elapsed_master_clocks = UINT64_MAX;
    before = timing;
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, 1U) ==
          MIPSNES2_HV_TIME_OVERFLOW);
    CHECK(memcmp(&timing, &before, sizeof(timing)) == 0);

    timing.elapsed_master_clocks = 0U;
    timing.frame_count = UINT64_MAX;
    timing.v_counter = MIPSNES2_HV_NTSC_SCANLINES - 1U;
    timing.h_master_clock = MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS - 1U;
    before = timing;
    CHECK(mipsnes2_hv_timing_advance(&timing, &cpu_io, 1U) ==
          MIPSNES2_HV_TIME_OVERFLOW);
    CHECK(memcmp(&timing, &before, sizeof(timing)) == 0);
    return 0;
}

int main(void)
{
    CHECK(test_initialization_and_validation() == 0);
    CHECK(test_horizontal_blank_and_line_boundary() == 0);
    CHECK(test_vblank_and_delayed_nmi_flag() == 0);
    CHECK(test_overscan_vblank_and_ntsc_short_line() == 0);
    CHECK(test_ntsc_frame_lengths_and_field_toggle() == 0);
    CHECK(test_frame_boundary_distance() == 0);
    CHECK(test_next_boundary_distance() == 0);
    CHECK(test_h_irq_normal_and_late_spill() == 0);
    CHECK(test_all_h_irq_values() == 0);
    CHECK(test_v_and_hv_irq_positions() == 0);
    CHECK(test_v_irq_after_short_line_and_suppressed_h153() == 0);
    CHECK(test_irq_range_and_enable_boundary() == 0);
    CHECK(test_auto_joypad_timing_and_snapshot() == 0);
    CHECK(test_auto_joypad_enable_phase_and_overscan() == 0);
    CHECK(test_pal_frame_and_chunk_determinism() == 0);
    CHECK(test_advance_errors() == 0);
    (void)puts("PASS: deterministic SNES H/V, interrupt and auto-joypad timing foundation");
    return 0;
}
