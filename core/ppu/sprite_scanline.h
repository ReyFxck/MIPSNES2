/* MIPSNES2 OBJ scanline selection. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_SPRITE_SCANLINE_H
#define MIPSNES2_PPU_SPRITE_SCANLINE_H

#include "core/ppu/oam_ports.h"

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

/* line_y is the sprite-addressed Y value, not the machine H/V counter.
 * first_sprite is supplied explicitly until priority-rotation timing is
 * modeled. This pure selection does not update $213E or render pixels.
 * On invalid arguments or undocumented OBJ size modes 6/7, output is intact.
 */
bool mipsnes2_ppu_obj_select_scanline(const mipsnes2_ppu_oam_ports *ports,
                                     uint8_t line_y, unsigned first_sprite,
                                     mipsnes2_ppu_obj_scanline *output);

#endif
