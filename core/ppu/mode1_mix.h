/* MIPSNES2 Mode 1 layer order. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_MODE1_MIX_H
#define MIPSNES2_PPU_MODE1_MIX_H

#include "core/ppu/mode0_mix.h"

/* BG4 is absent. BG3 high-priority override is $2105 bit 3.
 * Output uses the shared CGRAM-index/source line for color math.
 */
bool mipsnes2_ppu_mode1_mix_masked(
    const mipsnes2_ppu_bg2_scanline *bg[3],
    const mipsnes2_ppu_obj_pixels *obj, uint8_t screen_enable,
    bool bg3_priority, const uint8_t hidden[256],
    mipsnes2_ppu_mode0_line *output);

#endif
