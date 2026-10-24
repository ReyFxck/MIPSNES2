/* Deterministic 4bpp tile fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/oam_ports.h"
#include "core/ppu/tile4.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t vram[MIPSNES2_PPU_TILE4_VRAM_SIZE];
static uint8_t pixels[MIPSNES2_PPU_TILE4_PIXELS];

int main(void)
{
    mipsnes2_ppu_oam_sprite sprite;
    uint16_t word_address = 0U;
    unsigned i;

    (void)memset(vram, 0, sizeof(vram));
    (void)memset(pixels, 0x5a, sizeof(pixels));
    CHECK(mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram), 0U, pixels));
    for (i = 0U; i < 64U; ++i) {
        CHECK(pixels[i] == 0U);
    }
    (void)memset(vram, 0xff, 32U);
    CHECK(mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram), 0U, pixels));
    for (i = 0U; i < 64U; ++i) {
        CHECK(pixels[i] == 15U);
    }

    /* Four single-plane pixels at distinct columns, and a mixed final row. */
    (void)memset(vram, 0, 32U);
    vram[0] = 0x80U;
    vram[1] = 0x40U;
    vram[16] = 0x20U;
    vram[17] = 0x10U;
    vram[14] = 0x01U;
    vram[15] = 0x01U;
    vram[30] = 0x01U;
    vram[31] = 0x01U;
    CHECK(mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram), 0U, pixels));
    CHECK(pixels[0] == 1U && pixels[1] == 2U);
    CHECK(pixels[2] == 4U && pixels[3] == 8U && pixels[4] == 0U);
    CHECK(pixels[63] == 15U && pixels[56] == 0U);

    /* A tile fetch crossing the end of VRAM wraps to byte zero. */
    (void)memset(vram, 0, sizeof(vram));
    vram[0xfff0U] = 0x80U;
    vram[0U] = 0x80U;
    CHECK(mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram), 0x7ff8U,
                                         pixels));
    CHECK(pixels[0] == 5U && pixels[1] == 0U);
    CHECK(pixels[4U * 8U] == 0U);

    /* The OBJ address helper and the generic tile decoder use one word unit. */
    (void)memset(&sprite, 0, sizeof(sprite));
    sprite.character = 0x1feU;
    sprite.large = true;
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 2U, 0U,
                                            &word_address));
    CHECK(word_address == 0x1f00U);
    (void)memset(vram, 0, sizeof(vram));
    vram[0x3e00U] = 0x80U;
    vram[0x3e01U] = 0x80U;
    CHECK(mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram), word_address,
                                         pixels));
    CHECK(pixels[0] == 3U && pixels[1] == 0U);

    (void)memset(pixels, 0x5a, sizeof(pixels));
    CHECK(!mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram) - 1U, 0U,
                                          pixels));
    CHECK(!mipsnes2_ppu_decode_4bpp_tile(NULL, sizeof(vram), 0U,
                                          pixels));
    CHECK(!mipsnes2_ppu_decode_4bpp_tile(vram, sizeof(vram), 0U,
                                          NULL));
    for (i = 0U; i < 64U; ++i) {
        CHECK(pixels[i] == 0x5aU);
    }
    (void)puts("PPU 4bpp tile: OK");
    return 0;
}
