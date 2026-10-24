/* MIPSNES2 Mode 1 layer order. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/mode1_mix.h"

bool mipsnes2_ppu_mode1_mix_masked(
    const mipsnes2_ppu_bg2_scanline *bg[3],
    const mipsnes2_ppu_obj_pixels *obj, uint8_t screen_enable,
    bool bg3_priority, const uint8_t hidden[256],
    mipsnes2_ppu_mode0_line *output)
{
    static const uint8_t bg_rank[3][2] = {
        { 8U, 12U }, { 7U, 11U }, { 2U, 5U }
    };
    static const uint8_t obj_rank[4] = { 4U, 6U, 10U, 13U };
    unsigned bg_index;
    unsigned x;

    if (bg == NULL || output == NULL ||
        ((screen_enable & UINT8_C(0x10)) != 0U && obj == NULL)) {
        return false;
    }
    for (bg_index = 0U; bg_index < 3U; ++bg_index) {
        if ((screen_enable & (1U << bg_index)) != 0U &&
            bg[bg_index] == NULL) {
            return false;
        }
    }
    for (x = 0U; x < 256U; ++x) {
        uint8_t rank = 1U;
        uint8_t color = 0U;
        uint8_t source = 0U;

        for (bg_index = 0U; bg_index < 3U; ++bg_index) {
            const mipsnes2_ppu_bg2_pixel *pixel;
            uint8_t candidate;

            if ((screen_enable & (1U << bg_index)) == 0U ||
                (hidden != NULL &&
                 (hidden[x] & (1U << bg_index)) != 0U)) {
                continue;
            }
            pixel = &bg[bg_index]->pixels[x];
            candidate = bg_index == 2U && pixel->priority != 0U &&
                        bg3_priority ? 14U :
                        bg_rank[bg_index][pixel->priority & 1U];
            if (pixel->opaque && candidate > rank) {
                rank = candidate;
                color = pixel->color;
                source = (uint8_t)(bg_index + 1U);
            }
        }
        if ((screen_enable & UINT8_C(0x10)) != 0U &&
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
