/* MIPSNES2 4bpp BG scanline. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_BG4_SCANLINE_H
#define MIPSNES2_PPU_BG4_SCANLINE_H

#include "core/ppu/bg2_scanline.h"

/* Pure 4bpp BG line. Shares map/scroll fields with the 2bpp config;
 * palette_base is ignored because 4bpp palettes occupy CGRAM 0-127.
 * Invalid input leaves output unchanged.
 */
bool mipsnes2_ppu_bg4_render_scanline(
    const uint8_t *vram, size_t vram_size,
    const mipsnes2_ppu_bg2_config *config, uint16_t line_y,
    mipsnes2_ppu_bg2_scanline *output);

#endif
