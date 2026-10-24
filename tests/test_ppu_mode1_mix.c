/* Synthetic Mode 1 priority fixtures; no ROM or emulator source. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/mode1_mix.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static mipsnes2_ppu_bg2_scanline bg_storage[3];
static mipsnes2_ppu_obj_pixels objects;
static mipsnes2_ppu_mode0_line output;
static uint8_t hidden[256];

int main(void)
{
    const mipsnes2_ppu_bg2_scanline *bg[3] = {
        &bg_storage[0], &bg_storage[1], &bg_storage[2]
    };

    (void)memset(bg_storage, 0, sizeof(bg_storage));
    (void)memset(&objects, 0, sizeof(objects));
    (void)memset(hidden, 0, sizeof(hidden));
    bg_storage[0].pixels[0].opaque = true;
    bg_storage[0].pixels[0].color = 11U;
    bg_storage[1].pixels[0].opaque = true;
    bg_storage[1].pixels[0].color = 22U;
    bg_storage[2].pixels[0].opaque = true;
    bg_storage[2].pixels[0].color = 33U;
    objects.pixels[0].color = 129U;
    CHECK(mipsnes2_ppu_mode1_mix_masked(bg, &objects, 0x17U,
                                         false, NULL, &output));
    CHECK(output.pixels[0].color == 11U && output.pixels[0].source == 1U);
    bg_storage[1].pixels[0].priority = 1U;
    CHECK(mipsnes2_ppu_mode1_mix_masked(bg, &objects, 0x17U,
                                         false, NULL, &output));
    CHECK(output.pixels[0].color == 22U);
    objects.pixels[0].priority = 3U;
    CHECK(mipsnes2_ppu_mode1_mix_masked(bg, &objects, 0x17U,
                                         false, NULL, &output));
    CHECK(output.pixels[0].color == 129U && output.pixels[0].source == 5U);
    bg_storage[2].pixels[0].priority = 1U;
    CHECK(mipsnes2_ppu_mode1_mix_masked(bg, &objects, 0x17U,
                                         true, NULL, &output));
    CHECK(output.pixels[0].color == 33U && output.pixels[0].source == 3U);
    hidden[0] = 4U;
    CHECK(mipsnes2_ppu_mode1_mix_masked(bg, &objects, 0x17U,
                                         true, hidden, &output));
    CHECK(output.pixels[0].color == 129U);
    CHECK(mipsnes2_ppu_mode1_mix_masked(bg, &objects, 0U,
                                         false, NULL, &output));
    CHECK(output.pixels[0].color == 0U && output.pixels[0].source == 0U);
    (void)memset(&output, 0x5a, sizeof(output));
    bg[0] = NULL;
    CHECK(!mipsnes2_ppu_mode1_mix_masked(bg, &objects, 1U,
                                          false, NULL, &output));
    CHECK(output.pixels[0].color == UINT8_C(0x5a));
    (void)puts("PPU Mode 1 mixer: OK");
    return 0;
}
