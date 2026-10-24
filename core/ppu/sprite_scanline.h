/* MIPSNES2 OBJ scanline selection. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_SPRITE_SCANLINE_H
#define MIPSNES2_PPU_SPRITE_SCANLINE_H

#include "core/ppu/oam_ports.h"
#include "core/ppu/tile4.h"

enum { MIPSNES2_PPU_OBJ_RANGE_LIMIT = 32,
       MIPSNES2_PPU_OBJ_TIME_LIMIT = 34 };

typedef struct {
    uint8_t sprite_index;
    uint8_t source_tile_x;
    uint8_t source_tile_y;
    int16_t screen_x;
} mipsnes2_ppu_obj_sliver;

typedef struct {
    uint8_t range[MIPSNES2_PPU_OBJ_RANGE_LIMIT];
    mipsnes2_ppu_obj_sliver slivers[MIPSNES2_PPU_OBJ_TIME_LIMIT];
    uint8_t range_count;
    uint8_t sliver_count;
    bool range_over;
    bool time_over;
} mipsnes2_ppu_obj_scanline;

typedef struct {
    uint8_t color; /* 0 is transparent; otherwise 128 + 16*palette + 1..15. */
    uint8_t priority;
    uint8_t sprite_index;
} mipsnes2_ppu_obj_pixel;

typedef struct {
    mipsnes2_ppu_obj_pixel pixels[256];
} mipsnes2_ppu_obj_pixels;

/* line_y is the sprite-addressed Y value, not the machine H/V counter.
 * first_sprite is supplied explicitly until priority-rotation timing is
 * modeled. This pure selection does not update $213E or render pixels.
 * On invalid arguments or undocumented OBJ size modes 6/7, output is intact.
 */
bool mipsnes2_ppu_obj_select_scanline(const mipsnes2_ppu_oam_ports *ports,
                                     uint8_t line_y, unsigned first_sprite,
                                     mipsnes2_ppu_obj_scanline *output);

/* Composite the already selected slivers against caller-owned VRAM. Later
 * fetched (higher-priority) nonzero sprite pixels replace earlier ones.
 * This does not mix OBJ with BG, windows, color math or the output surface.
 * Invalid inputs leave output unchanged.
 */
bool mipsnes2_ppu_obj_compose_scanline(
    const mipsnes2_ppu_oam_ports *ports,
    const uint8_t *vram, size_t vram_size, uint8_t line_y,
    const mipsnes2_ppu_obj_scanline *selection,
    mipsnes2_ppu_obj_pixels *output);

#endif
