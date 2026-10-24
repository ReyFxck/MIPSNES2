/* MIPSNES2 Mode 0 color arithmetic. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/color_math.h"

static uint8_t math_channel(uint8_t main, uint8_t addend,
                            bool subtract, bool half)
{
    int result = subtract ? (int)main - (int)addend :
                            (int)main + (int)addend;

    if (half) {
        result /= 2;
    }
    if (result < 0) {
        result = 0;
    } else if (result > 31) {
        result = 31;
    }
    return (uint8_t)result;
}

bool mipsnes2_ppu_color_math_line(
    const mipsnes2_ppu_mode0_line *main_indices,
    const mipsnes2_ppu_mode0_color_line *main_raw,
    const mipsnes2_ppu_mode0_line *sub_indices,
    const mipsnes2_ppu_mode0_color_line *sub_raw,
    uint16_t fixed_color, uint8_t select, uint8_t control,
    uint8_t brightness, bool forced_blank,
    mipsnes2_ppu_mode0_color_line *output)
{
    unsigned x;

    if (main_indices == NULL || main_raw == NULL || sub_indices == NULL ||
        sub_raw == NULL || output == NULL || fixed_color > UINT16_C(0x7fff) ||
        brightness > 15U) {
        return false;
    }
    for (x = 0U; x < 256U; ++x) {
        const mipsnes2_ppu_mode0_pixel *pixel = &main_indices->pixels[x];
        uint8_t enable_bit = pixel->source == 0U ? UINT8_C(0x20) :
            pixel->source <= 4U ? (uint8_t)(1U << (pixel->source - 1U)) :
            pixel->source == 5U && pixel->color >= UINT8_C(0xc0) ?
                UINT8_C(0x10) : 0U;
        bool use_sub = (select & UINT8_C(0x02)) != 0U;
        bool enabled = (control & enable_bit) != 0U;
        bool half = (control & UINT8_C(0x40)) != 0U &&
                    !(use_sub && sub_indices->pixels[x].source == 0U);
        uint16_t main_color = main_raw->pixels[x];
        uint16_t addend = use_sub ? sub_raw->pixels[x] : fixed_color;
        uint16_t result = 0U;
        unsigned channel;

        for (channel = 0U; channel < 3U; ++channel) {
            unsigned shift = channel * 5U;
            uint8_t component = (uint8_t)((main_color >> shift) & 31U);

            if (enabled) {
                component = math_channel(
                    component, (uint8_t)((addend >> shift) & 31U),
                    (control & UINT8_C(0x80)) != 0U, half);
            }
            component = (uint8_t)((component * (unsigned)brightness) / 15U);
            result |= (uint16_t)((uint16_t)component << shift);
        }
        output->pixels[x] = forced_blank ? 0U : result;
    }
    return true;
}
