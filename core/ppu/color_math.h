/* MIPSNES2 Mode 0 color arithmetic. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_COLOR_MATH_H
#define MIPSNES2_PPU_COLOR_MATH_H

#include "core/ppu/mode0_mix.h"

/* select is $2130 (only bit 1 used); control is $2131.
 * Main/sub colors are unattenuated BGR555. Brightness applies after math.
 * Color windows and direct color are outside this reference subset.
 * Invalid arguments leave output unchanged.
 */
bool mipsnes2_ppu_color_math_line(
    const mipsnes2_ppu_mode0_line *main_indices,
    const mipsnes2_ppu_mode0_color_line *main_raw,
    const mipsnes2_ppu_mode0_line *sub_indices,
    const mipsnes2_ppu_mode0_color_line *sub_raw,
    uint16_t fixed_color, uint8_t select, uint8_t control,
    uint8_t brightness, bool forced_blank,
    mipsnes2_ppu_mode0_color_line *output);

/* Optional color-window predicate enables $2130 region bits 4-7. */
bool mipsnes2_ppu_color_math_line_windowed(
    const mipsnes2_ppu_mode0_line *main_indices,
    const mipsnes2_ppu_mode0_color_line *main_raw,
    const mipsnes2_ppu_mode0_line *sub_indices,
    const mipsnes2_ppu_mode0_color_line *sub_raw,
    uint16_t fixed_color, uint8_t select, uint8_t control,
    uint8_t brightness, bool forced_blank,
    const uint8_t color_window_inside[256],
    mipsnes2_ppu_mode0_color_line *output);

#endif
