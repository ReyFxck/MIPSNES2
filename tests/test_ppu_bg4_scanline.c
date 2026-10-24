/* Synthetic 4bpp BG fixtures; no ROM or emulator source. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/bg4_scanline.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t vram[65536];
static mipsnes2_ppu_bg2_scanline line;

static void map_word(unsigned word, uint16_t entry)
{
    vram[(word & 0x7fffU) * 2U] = (uint8_t)entry;
    vram[(word & 0x7fffU) * 2U + 1U] = (uint8_t)(entry >> 8);
}

int main(void)
{
    mipsnes2_ppu_bg2_config config;

    (void)memset(&config, 0, sizeof(config));
    (void)memset(vram, 0, sizeof(vram));
    config.screen_base_word = UINT16_C(0x400);
    map_word(0x400U, UINT16_C(0x2c00)); /* Palette 3, priority 1. */
    vram[0] = UINT8_C(0x80);
    vram[1] = UINT8_C(0x40);
    vram[16] = UINT8_C(0x20);
    vram[17] = UINT8_C(0x10);
    CHECK(mipsnes2_ppu_bg4_render_scanline(vram, sizeof(vram),
                                            &config, 0U, &line));
    CHECK(line.pixels[0].opaque && line.pixels[0].color == 49U &&
          line.pixels[0].priority == 1U);
    CHECK(line.pixels[1].opaque && line.pixels[1].color == 50U);
    CHECK(line.pixels[2].opaque && line.pixels[2].color == 52U);
    CHECK(line.pixels[3].opaque && line.pixels[3].color == 56U);
    CHECK(!line.pixels[4].opaque && line.pixels[4].color == 48U);

    /* Map B in a 64x32 layout, then D in a 64x64 layout. */
    map_word(0x800U, 1U);
    vram[32] = UINT8_C(0x80);
    config.screen_size = 1U;
    config.scroll_x = 256U;
    CHECK(mipsnes2_ppu_bg4_render_scanline(vram, sizeof(vram),
                                            &config, 0U, &line));
    CHECK(line.pixels[0].opaque && line.pixels[0].color == 1U);
    config.screen_size = 3U;
    config.scroll_y = 256U;
    map_word(0x1000U, 2U);
    vram[64] = UINT8_C(0x80);
    CHECK(mipsnes2_ppu_bg4_render_scanline(vram, sizeof(vram),
                                            &config, 0U, &line));
    CHECK(line.pixels[0].opaque && line.pixels[0].color == 1U);

    /* Flipped 16x16 entry selects character 17, row 7, bit 0. */
    config.screen_size = 0U;
    config.scroll_x = 0U;
    config.scroll_y = 0U;
    config.tile_size16 = true;
    map_word(0x400U, UINT16_C(0xc000));
    vram[17U * 32U + 14U] = 1U;
    CHECK(mipsnes2_ppu_bg4_render_scanline(vram, sizeof(vram),
                                            &config, 0U, &line));
    CHECK(line.pixels[0].opaque && line.pixels[0].color == 1U);
    CHECK(!line.pixels[1].opaque);

    (void)memset(&line, 0x5a, sizeof(line));
    CHECK(!mipsnes2_ppu_bg4_render_scanline(vram, sizeof(vram) - 1U,
                                             &config, 0U, &line));
    config.screen_size = 4U;
    CHECK(!mipsnes2_ppu_bg4_render_scanline(vram, sizeof(vram),
                                             &config, 0U, &line));
    CHECK(line.pixels[0].color == UINT8_C(0x5a));
    (void)puts("PPU 4bpp BG scanline: OK");
    return 0;
}
