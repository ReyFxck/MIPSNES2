/* MIPSNES2 original counter-register tests. See LICENSE for GPL-3.0 terms. */
#include <stdio.h>
#include <string.h>

#include "core/ppu/counters.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static int test_horizontal_durations(void)
{
    unsigned region;
    unsigned short_line;

    for (region = 0U; region < 2U; ++region) {
        for (short_line = 0U; short_line < 2U; ++short_line) {
            mipsnes2_hv_timing timing;
            mipsnes2_cpu_io io;
            unsigned dot;
            unsigned clock = 0U;
            uint16_t invalid_output = UINT16_MAX;

            mipsnes2_cpu_io_init(&io, 2U);
            CHECK(mipsnes2_hv_timing_init(
                &timing, (mipsnes2_hv_standard)region, false, &io) ==
                MIPSNES2_HV_OK);
            timing.v_counter = 240U;
            timing.field = short_line != 0U;
            for (dot = 0U; dot < 340U; ++dot) {
                unsigned duration = 4U;
                unsigned subclock;

                if (!(region == 0U && short_line != 0U) &&
                    (dot == 323U || dot == 327U)) {
                    duration = 6U;
                }
                for (subclock = 0U; subclock < duration; ++subclock) {
                    uint16_t horizontal = UINT16_MAX;

                    timing.h_master_clock = (uint16_t)clock++;
                    CHECK(mipsnes2_hv_ppu_horizontal(&timing, &horizontal));
                    CHECK(horizontal == dot);
                }
            }
            CHECK(clock == mipsnes2_hv_scanline_master_clocks(&timing));
            timing.h_master_clock = (uint16_t)clock;
            CHECK(!mipsnes2_hv_ppu_horizontal(&timing, &invalid_output));
            CHECK(invalid_output == UINT16_MAX);
        }
    }
    return 0;
}

static int test_registers_and_selectors(void)
{
    mipsnes2_ppu_counters counters;
    mipsnes2_ppu_counters before;
    mipsnes2_hv_timing timing;
    mipsnes2_cpu_io io;
    uint8_t value = UINT8_C(0x5a);
    uint16_t position;

    mipsnes2_cpu_io_init(&io, 2U);
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_PAL,
                                  false, &io) == MIPSNES2_HV_OK);
    mipsnes2_ppu_counters_init(NULL, 3U, 0U);
    mipsnes2_ppu_counters_init(&counters, UINT8_C(0xf3), UINT8_C(0xa4));
    CHECK(counters.version == 3U && counters.open_bus2 == UINT8_C(0xa4));
    CHECK(counters.horizontal == 0U && counters.vertical == 0U);
    before = counters;
    CHECK(!mipsnes2_hv_ppu_horizontal(NULL, &position));
    CHECK(!mipsnes2_hv_ppu_horizontal(&timing, NULL));
    CHECK(!mipsnes2_ppu_counters_capture(NULL, &timing));
    CHECK(!mipsnes2_ppu_counters_capture(&counters, NULL));
    CHECK(!mipsnes2_ppu_counters_read(&counters, &timing, true,
                                      UINT16_C(0x213b), 0U, &value));
    CHECK(value == UINT8_C(0x5a) && memcmp(&before, &counters, sizeof(before)) == 0);
    CHECK(!mipsnes2_ppu_counters_read(&counters, &timing, true,
                                      UINT16_C(0x2137), 0U, NULL));
    counters.initialized = false;
    CHECK(!mipsnes2_ppu_counters_capture(&counters, &timing));
    CHECK(!mipsnes2_ppu_counters_read(&counters, &timing, true,
                                      UINT16_C(0x2137), 0U, &value));
    counters = before;
    timing.v_counter = 312U;
    CHECK(!mipsnes2_ppu_counters_capture(&counters, &timing));
    CHECK(memcmp(&before, &counters, sizeof(before)) == 0);
    timing.v_counter = 0U;

    /* Exhaust all valid normal-line H positions and all non-interlaced PAL V values. */
    for (position = 0U; position < 340U; ++position) {
        uint16_t vertical = (uint16_t)(position % 312U);
        unsigned reads;

        timing.h_master_clock = (uint16_t)(position * 4U +
            (position > 323U ? 2U : 0U) + (position > 327U ? 2U : 0U));
        timing.v_counter = vertical;
        CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                         UINT16_C(0x2137), UINT8_C(0x5a), &value));
        CHECK(value == UINT8_C(0x5a));
        CHECK(counters.horizontal == position && counters.vertical == vertical);
        CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                         UINT16_C(0x213f), 0U, &value));
        CHECK((value & UINT8_C(0x50)) == UINT8_C(0x50));
        CHECK(!counters.latched);
        for (reads = 0U; reads < 4U; ++reads) {
            uint8_t previous = counters.open_bus2;

            CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                             UINT16_C(0x213c), 0U, &value));
            if ((reads & 1U) == 0U) {
                CHECK(value == (uint8_t)position);
            } else {
                CHECK(value == (uint8_t)((previous & UINT8_C(0xfe)) | (position >> 8)));
            }
            previous = value;
            CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                             UINT16_C(0x213d), 0U, &value));
            CHECK(value == ((reads & 1U) == 0U ? (uint8_t)vertical :
                             (uint8_t)((previous & UINT8_C(0xfe)) | (vertical >> 8))));
        }
    }

    /* Relatching never resets either byte selector or the PPU2 bus latch. */
    timing.h_master_clock = 400U;
    timing.v_counter = 300U;
    CHECK(mipsnes2_ppu_counters_capture(&counters, &timing));
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x213c), 0U, &value));
    CHECK(counters.horizontal_high && !counters.vertical_high);
    timing.h_master_clock = 1200U;
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x2137), UINT8_C(0xff), &value));
    CHECK(counters.horizontal == 300U && counters.horizontal_high);
    CHECK(counters.open_bus2 == 100U);
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x213c), 0U, &value));
    CHECK(value == 101U);
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x213d), 0U, &value));
    CHECK(value == 44U && counters.vertical_high);
    before = counters;
    timing.h_master_clock = 0U;
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, false,
                                     UINT16_C(0x2137), UINT8_C(0xff), &value));
    CHECK(memcmp(&before, &counters, sizeof(before)) == 0);
    timing.field = true;
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, false,
                                     UINT16_C(0x213f), 0U, &value));
    CHECK(value == UINT8_C(0xf3));
    CHECK(counters.latched && !counters.horizontal_high && !counters.vertical_high);
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x213f), 0U, &value));
    CHECK(value == UINT8_C(0xf3) && !counters.latched);
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x213f), 0U, &value));
    CHECK(value == UINT8_C(0xb3));
    timing.standard = MIPSNES2_HV_STANDARD_NTSC;
    timing.v_counter = 0U;
    timing.field = false;
    CHECK(mipsnes2_ppu_counters_read(&counters, &timing, true,
                                     UINT16_C(0x213f), 0U, &value));
    CHECK(value == UINT8_C(0x23));
    CHECK(mipsnes2_ppu_counters_write_ignored(UINT16_C(0x2137)));
    CHECK(mipsnes2_ppu_counters_write_ignored(UINT16_C(0x213c)));
    CHECK(mipsnes2_ppu_counters_write_ignored(UINT16_C(0x213d)));
    CHECK(mipsnes2_ppu_counters_write_ignored(UINT16_C(0x213f)));
    CHECK(mipsnes2_ppu_counters_write_ignored(UINT16_C(0x213e)));
    return 0;
}

int main(void)
{
    CHECK(test_horizontal_durations() == 0);
    CHECK(test_registers_and_selectors() == 0);
    (void)puts("PASS: non-interlaced PPU H/V counter registers and long dots");
    return 0;
}
