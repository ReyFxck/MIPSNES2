/* MIPSNES2 4bpp BG scanline. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/bg4_scanline.h"

#include <string.h>

bool mipsnes2_ppu_bg4_render_scanline(
    const uint8_t *vram, size_t vram_size,
    const mipsnes2_ppu_bg2_config *config, uint16_t line_y,
    mipsnes2_ppu_bg2_scanline *output)
{
    mipsnes2_ppu_bg2_scanline rendered;
    unsigned tile_size;
    unsigned map_columns;
    unsigned width_mask;
    unsigned height_mask;
    unsigned screen_x;

    if (vram == NULL || vram_size < 65536U || config == NULL ||
        output == NULL || config->screen_base_word > UINT16_C(0x7fff) ||
        config->tile_base_word > UINT16_C(0x7fff) ||
        config->screen_size > 3U) {
        return false;
    }
    tile_size = config->tile_size16 ? 16U : 8U;
    map_columns = (config->screen_size & 1U) != 0U ? 2U : 1U;
    width_mask = map_columns * 32U * tile_size - 1U;
    height_mask = ((config->screen_size & 2U) != 0U ? 2U : 1U) *
                  32U * tile_size - 1U;
    (void)memset(&rendered, 0, sizeof(rendered));
    for (screen_x = 0U; screen_x < 256U; ++screen_x) {
        unsigned bg_x = ((unsigned)config->scroll_x + screen_x) & width_mask;
        unsigned bg_y = ((unsigned)config->scroll_y + line_y) & height_mask;
        unsigned tile_x = bg_x / tile_size;
        unsigned tile_y = bg_y / tile_size;
        unsigned map_number = (tile_y / 32U) * map_columns + tile_x / 32U;
        unsigned map_word = ((unsigned)config->screen_base_word +
                             map_number * 1024U +
                             (tile_y & 31U) * 32U + (tile_x & 31U)) & 0x7fffU;
        unsigned map_byte = map_word * 2U;
        unsigned entry = (unsigned)vram[map_byte] |
                         ((unsigned)vram[map_byte + 1U] << 8);
        unsigned inner_x = bg_x & (tile_size - 1U);
        unsigned inner_y = bg_y & (tile_size - 1U);
        unsigned character = entry & 0x3ffU;
        unsigned tile_byte;
        unsigned bit;
        unsigned color;

        if ((entry & 0x4000U) != 0U) {
            inner_x = tile_size - 1U - inner_x;
        }
        if ((entry & 0x8000U) != 0U) {
            inner_y = tile_size - 1U - inner_y;
        }
        if (config->tile_size16) {
            character = (character + (inner_x >> 3) +
                         ((inner_y >> 3) << 4)) & 0x3ffU;
        }
        tile_byte = (((unsigned)config->tile_base_word +
                      character * 16U) * 2U + (inner_y & 7U) * 2U) & 0xffffU;
        bit = 7U - (inner_x & 7U);
        color = ((unsigned)vram[tile_byte] >> bit) & 1U;
        color |= (((unsigned)vram[(tile_byte + 1U) & 0xffffU] >> bit) & 1U)
                 << 1;
        color |= (((unsigned)vram[(tile_byte + 16U) & 0xffffU] >> bit) & 1U)
                 << 2;
        color |= (((unsigned)vram[(tile_byte + 17U) & 0xffffU] >> bit) & 1U)
                 << 3;
        rendered.pixels[screen_x].color =
            (uint8_t)(((entry >> 10) & 7U) * 16U + color);
        rendered.pixels[screen_x].priority = (uint8_t)((entry >> 13) & 1U);
        rendered.pixels[screen_x].opaque = color != 0U;
    }
    *output = rendered;
    return true;
}
