/* MIPSNES2 SNES 4bpp tile decoding. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/tile4.h"

bool mipsnes2_ppu_decode_4bpp_tile(const uint8_t *vram, size_t vram_size,
                                   uint16_t word_address,
                                   uint8_t pixels[MIPSNES2_PPU_TILE4_PIXELS])
{
    unsigned base;
    unsigned y;

    if (vram == NULL || pixels == NULL ||
        vram_size < MIPSNES2_PPU_TILE4_VRAM_SIZE) {
        return false;
    }
    base = ((unsigned)word_address & 0x7fffU) * 2U;
    for (y = 0U; y < 8U; ++y) {
        uint8_t plane0 = vram[(base + y * 2U) & 0xffffU];
        uint8_t plane1 = vram[(base + y * 2U + 1U) & 0xffffU];
        uint8_t plane2 = vram[(base + 16U + y * 2U) & 0xffffU];
        uint8_t plane3 = vram[(base + 17U + y * 2U) & 0xffffU];
        unsigned x;

        for (x = 0U; x < 8U; ++x) {
            unsigned bit = 7U - x;

            pixels[y * 8U + x] = (uint8_t)(((plane0 >> bit) & 1U) |
                (((plane1 >> bit) & 1U) << 1) |
                (((plane2 >> bit) & 1U) << 2) |
                (((plane3 >> bit) & 1U) << 3));
        }
    }
    return true;
}
