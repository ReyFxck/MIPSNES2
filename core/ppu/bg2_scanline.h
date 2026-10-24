/* MIPSNES2 2bpp BG scanline foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_BG2_SCANLINE_H
#define MIPSNES2_PPU_BG2_SCANLINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t screen_base_word;
    uint16_t tile_base_word;
    uint16_t scroll_x;
    uint16_t scroll_y;
    uint8_t screen_size; /* BGnSC bits 0-1. */
    uint8_t palette_base; /* Mode 0 BG1..4: 0, 32, 64 or 96. */
    bool tile_size16;
} mipsnes2_ppu_bg2_config;

typedef struct {
    uint8_t color; /* CGRAM index if opaque. */
    uint8_t priority;
    bool opaque;
} mipsnes2_ppu_bg2_pixel;

typedef struct {
    mipsnes2_ppu_bg2_pixel pixels[256];
} mipsnes2_ppu_bg2_scanline;

/* Pure Mode 0-style 2bpp BG line; no register latches, windows, OBJ mixing
 * or presentation. Caller supplies decoded base addresses and scroll values.
 * Invalid input leaves output untouched.
 */
bool mipsnes2_ppu_bg2_render_scanline(
    const uint8_t *vram, size_t vram_size,
    const mipsnes2_ppu_bg2_config *config, uint16_t line_y,
    mipsnes2_ppu_bg2_scanline *output);

#endif
