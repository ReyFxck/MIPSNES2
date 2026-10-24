/* Synthetic Mode 0 color arithmetic vectors. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/color_math.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static mipsnes2_ppu_mode0_line main_index;
static mipsnes2_ppu_mode0_line sub_index;
static mipsnes2_ppu_mode0_color_line main_raw;
static mipsnes2_ppu_mode0_color_line sub_raw;
static mipsnes2_ppu_mode0_color_line output;
static uint8_t color_window[256];

int main(void)
{
    (void)memset(&main_index, 0, sizeof(main_index));
    (void)memset(&sub_index, 0, sizeof(sub_index));
    (void)memset(&main_raw, 0, sizeof(main_raw));
    (void)memset(&sub_raw, 0, sizeof(sub_raw));
    main_index.pixels[0].source = 1U;
    sub_index.pixels[0].source = 2U;
    main_raw.pixels[0] = UINT16_C(0x294a); /* B10 G10 R10. */
    sub_raw.pixels[0] = UINT16_C(0x14a5);  /* B5 G5 R5. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, 1U, 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x3def)); /* 15 each. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, UINT8_C(0x41), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x1ce7)); /* 7 each. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, UINT8_C(0x81), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x14a5)); /* 5 each. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, UINT8_C(0xc1), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x0842)); /* 2 each. */

    main_raw.pixels[0] = UINT16_C(0x7fff);
    sub_raw.pixels[0] = UINT16_C(0x7fff);
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, 1U, 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x7fff)); /* Saturation. */
    main_raw.pixels[0] = UINT16_C(0x7a85); /* B30 G20 R5. */
    sub_raw.pixels[0] = UINT16_C(0x1047);  /* B4 G2 R7. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, 1U, 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x7ecc)); /* B31 G22 R12. */
    main_raw.pixels[0] = UINT16_C(0x0003);
    sub_raw.pixels[0] = UINT16_C(0x001f);
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, UINT8_C(0x81), 15U, false, &output));
    CHECK(output.pixels[0] == 0U); /* Negative clamp. */

    main_raw.pixels[0] = UINT16_C(0x0010);
    sub_raw.pixels[0] = UINT16_C(0x0008);
    sub_index.pixels[0].source = 0U;
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, UINT8_C(0x41), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x0018)); /* Sub backdrop skips half. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), 0U, UINT8_C(0x41), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x000c)); /* Fixed-color half applies. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, 1U, 8U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x000c)); /* Brightness after add. */
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        2U, 1U, 15U, true, &output));
    CHECK(output.pixels[0] == 0U);

    main_index.pixels[0].source = 5U;
    main_index.pixels[0].color = UINT8_C(0xbf);
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), 0U, UINT8_C(0x10), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x0010));
    main_index.pixels[0].color = UINT8_C(0xc0);
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), 0U, UINT8_C(0x10), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x0018));
    main_index.pixels[0].source = 0U;
    CHECK(mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), 0U, UINT8_C(0x20), 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x0018));

    main_index.pixels[0].source = 1U;
    main_index.pixels[1].source = 1U;
    main_raw.pixels[0] = UINT16_C(0x0010);
    main_raw.pixels[1] = UINT16_C(0x0010);
    color_window[0] = 1U;
    CHECK(mipsnes2_ppu_color_math_line_windowed(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), UINT8_C(0x80), UINT8_C(0x41), 15U,
        false, color_window, &output));
    CHECK(output.pixels[0] == 8U && output.pixels[1] == 12U);
    CHECK(mipsnes2_ppu_color_math_line_windowed(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), UINT8_C(0x40), UINT8_C(0x41), 15U,
        false, color_window, &output));
    CHECK(output.pixels[0] == 12U && output.pixels[1] == 8U);
    CHECK(mipsnes2_ppu_color_math_line_windowed(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), UINT8_C(0x20), UINT8_C(0x41), 15U,
        false, color_window, &output));
    CHECK(output.pixels[0] == 16U && output.pixels[1] == 12U);
    CHECK(mipsnes2_ppu_color_math_line_windowed(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), UINT8_C(0x30), UINT8_C(0x41), 15U,
        false, color_window, &output));
    CHECK(output.pixels[0] == 16U && output.pixels[1] == 16U);
    CHECK(mipsnes2_ppu_color_math_line_windowed(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), UINT8_C(0xc0), UINT8_C(0x41), 15U,
        false, color_window, &output));
    CHECK(output.pixels[0] == 8U && output.pixels[1] == 8U);
    CHECK(mipsnes2_ppu_color_math_line_windowed(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x0008), UINT8_C(0xf0), UINT8_C(0x41), 15U,
        false, color_window, &output));
    CHECK(output.pixels[0] == 0U && output.pixels[1] == 0U);

    output.pixels[0] = UINT16_C(0x1234);
    CHECK(!mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw, 0U,
        0U, 0U, 16U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x1234));
    CHECK(!mipsnes2_ppu_color_math_line(
        &main_index, &main_raw, &sub_index, &sub_raw,
        UINT16_C(0x8000), 0U, 0U, 15U, false, &output));
    CHECK(output.pixels[0] == UINT16_C(0x1234));
    (void)puts("PPU Mode 0 color math: OK");
    return 0;
}
