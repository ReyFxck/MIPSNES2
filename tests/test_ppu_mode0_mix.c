/* Synthetic Mode 0 priority vectors. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/mode0_mix.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static mipsnes2_ppu_bg2_scanline backgrounds[4];
static mipsnes2_ppu_obj_pixels objects;
static mipsnes2_ppu_mode0_line output;

int main(void)
{
    const mipsnes2_ppu_bg2_scanline *bg[4] = {
        &backgrounds[0], &backgrounds[1], &backgrounds[2], &backgrounds[3]
    };
    unsigned i;

    (void)memset(backgrounds, 0, sizeof(backgrounds));
    (void)memset(&objects, 0, sizeof(objects));
    for (i = 0U; i < 4U; ++i) {
        backgrounds[i].pixels[0].opaque = true;
        backgrounds[i].pixels[0].color = (uint8_t)(10U + i);
        backgrounds[i].pixels[0].priority = 1U;
    }
    objects.pixels[0].color = 129U;
    objects.pixels[0].priority = 2U;
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, UINT8_C(0x1f), &output));
    CHECK(output.pixels[0].color == 10U && output.pixels[0].source == 1U);
    CHECK(output.pixels[1].color == 0U && output.pixels[1].source == 0U);
    objects.pixels[0].priority = 3U;
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, UINT8_C(0x1f), &output));
    CHECK(output.pixels[0].color == 129U && output.pixels[0].source == 5U);
    objects.pixels[0].priority = 2U;
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, UINT8_C(0x1e), &output));
    CHECK(output.pixels[0].color == 11U && output.pixels[0].source == 2U);
    backgrounds[1].pixels[0].opaque = false;
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, UINT8_C(0x1e), &output));
    CHECK(output.pixels[0].color == 129U && output.pixels[0].source == 5U);
    objects.pixels[0].priority = 0U;
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, UINT8_C(0x1c), &output));
    CHECK(output.pixels[0].color == 12U && output.pixels[0].source == 3U);
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, 0U, &output));
    CHECK(output.pixels[0].color == 0U && output.pixels[0].source == 0U);
    (void)memset(&output, 0x5a, sizeof(output));
    bg[0] = NULL;
    CHECK(!mipsnes2_ppu_mode0_mix(bg, &objects, 1U, &output));
    CHECK(output.pixels[0].color == UINT8_C(0x5a));
    CHECK(mipsnes2_ppu_mode0_mix(bg, &objects, 0U, &output));
    (void)puts("PPU Mode 0 mixer: OK");
    return 0;
}
