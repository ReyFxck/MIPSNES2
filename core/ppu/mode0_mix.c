/* MIPSNES2 Mode 0 layer-order reference. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/mode0_mix.h"
#include "core/ppu/cgram_ports.h"

/* Bottom-to-top ranks from the Mode 0 priority chart. */
static const uint8_t bg_rank[4][2] = {
    { 9U, 12U }, { 8U, 11U }, { 3U, 6U }, { 2U, 5U }
};
static const uint8_t obj_rank[4] = { 4U, 7U, 10U, 13U };

bool mipsnes2_ppu_mode0_mix(
    const mipsnes2_ppu_bg2_scanline *bg[4],
    const mipsnes2_ppu_obj_pixels *obj, uint8_t main_enable,
    mipsnes2_ppu_mode0_line *output)
{
    return mipsnes2_ppu_mode0_mix_masked(bg, obj, main_enable, NULL, output);
}

bool mipsnes2_ppu_mode0_mix_masked(
    const mipsnes2_ppu_bg2_scanline *bg[4],
    const mipsnes2_ppu_obj_pixels *obj, uint8_t main_enable,
    const uint8_t hidden[256], mipsnes2_ppu_mode0_line *output)
{
    unsigned x;
    unsigned layer;

    if (bg == NULL || output == NULL ||
        ((main_enable & UINT8_C(0x10)) != 0U && obj == NULL)) {
        return false;
    }
    for (layer = 0U; layer < 4U; ++layer) {
        if ((main_enable & (1U << layer)) != 0U && bg[layer] == NULL) {
            return false;
        }
    }
    for (x = 0U; x < 256U; ++x) {
        uint8_t rank = 1U;
        uint8_t color = 0U;
        uint8_t source = 0U;

        for (layer = 0U; layer < 4U; ++layer) {
            const mipsnes2_ppu_bg2_pixel *pixel;
            uint8_t candidate;

            if ((main_enable & (1U << layer)) == 0U ||
                (hidden != NULL && (hidden[x] & (1U << layer)) != 0U)) {
                continue;
            }
            pixel = &bg[layer]->pixels[x];
            candidate = bg_rank[layer][pixel->priority & 1U];
            if (pixel->opaque && candidate > rank) {
                rank = candidate;
                color = pixel->color;
                source = (uint8_t)(layer + 1U);
            }
        }
        if ((main_enable & UINT8_C(0x10)) != 0U &&
            (hidden == NULL || (hidden[x] & UINT8_C(0x10)) == 0U) &&
            obj->pixels[x].color != 0U) {
            uint8_t candidate = obj_rank[obj->pixels[x].priority & 3U];

            if (candidate > rank) {
                color = obj->pixels[x].color;
                source = 5U;
            }
        }
        output->pixels[x].color = color;
        output->pixels[x].source = source;
    }
    return true;
}

bool mipsnes2_ppu_mode0_resolve_color(
    const mipsnes2_ppu_mode0_line *indices, const uint8_t *cgram,
    size_t cgram_size, uint8_t brightness, bool forced_blank,
    mipsnes2_ppu_mode0_color_line *output)
{
    uint8_t scaled[32];
    unsigned component;
    unsigned x;

    if (indices == NULL || cgram == NULL ||
        cgram_size < MIPSNES2_PPU_CGRAM_SIZE || brightness > 15U ||
        output == NULL) {
        return false;
    }
    for (component = 0U; component < 32U; ++component) {
        scaled[component] =
            (uint8_t)((component * (unsigned)brightness) / 15U);
    }
    for (x = 0U; x < 256U; ++x) {
        size_t address = (size_t)indices->pixels[x].color * 2U;
        uint16_t color = (uint16_t)cgram[address] |
                         (uint16_t)((uint16_t)cgram[address + 1U] << 8);

        output->pixels[x] = forced_blank ? 0U :
            (uint16_t)((uint16_t)scaled[color & 31U] |
                       (uint16_t)((uint16_t)scaled[(color >> 5) & 31U] << 5) |
                       (uint16_t)((uint16_t)scaled[(color >> 10) & 31U] << 10));
    }
    return true;
}

bool mipsnes2_ppu_mode0_resolve_sub_color(
    const mipsnes2_ppu_mode0_line *indices, const uint8_t *cgram,
    size_t cgram_size, uint16_t fixed_color,
    mipsnes2_ppu_mode0_color_line *output)
{
    unsigned x;

    if (fixed_color > UINT16_C(0x7fff) ||
        !mipsnes2_ppu_mode0_resolve_color(
            indices, cgram, cgram_size, 15U, false, output)) {
        return false;
    }
    for (x = 0U; x < 256U; ++x) {
        if (indices->pixels[x].source == 0U) {
            output->pixels[x] = fixed_color;
        }
    }
    return true;
}
