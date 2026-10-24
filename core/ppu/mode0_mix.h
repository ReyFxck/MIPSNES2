/* MIPSNES2 Mode 0 layer-order reference. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_MODE0_MIX_H
#define MIPSNES2_PPU_MODE0_MIX_H

#include "core/ppu/bg2_scanline.h"
#include "core/ppu/sprite_scanline.h"

typedef struct {
    uint8_t color;  /* CGRAM index, including backdrop index zero. */
    uint8_t source; /* 0 backdrop, 1..4 BG1..4, 5 OBJ. */
} mipsnes2_ppu_mode0_pixel;

typedef struct {
    mipsnes2_ppu_mode0_pixel pixels[256];
} mipsnes2_ppu_mode0_line;

typedef struct {
    uint16_t pixels[256]; /* Nominal BGR555, not analog video output. */
} mipsnes2_ppu_mode0_color_line;

/* main_enable uses $212C bits 0..4. Null disabled layers are accepted.
 * This selects a CGRAM index only: no windows, sub screen, color math,
 * brightness or RGB lookup. Invalid enabled layer pointers leave output
 * unchanged.
 */
bool mipsnes2_ppu_mode0_mix(
    const mipsnes2_ppu_bg2_scanline *bg[4],
    const mipsnes2_ppu_obj_pixels *obj, uint8_t main_enable,
    mipsnes2_ppu_mode0_line *output);

/* Resolve a complete index line against 512 caller-owned CGRAM bytes.
 * Brightness 0..15 scales each 5-bit channel by brightness/15, rounding
 * down. Forced blank produces black. This is a digital approximation of
 * $2100's output brightness, not an analog DAC or mid-line timing model.
 * Invalid arguments leave output unchanged.
 */
bool mipsnes2_ppu_mode0_resolve_color(
    const mipsnes2_ppu_mode0_line *indices, const uint8_t *cgram,
    size_t cgram_size, uint8_t brightness, bool forced_blank,
    mipsnes2_ppu_mode0_color_line *output);

#endif
