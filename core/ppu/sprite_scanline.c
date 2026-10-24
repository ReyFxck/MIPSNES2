/* MIPSNES2 OBJ scanline selection. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/sprite_scanline.h"

#include <string.h>

static int16_t signed_x(uint16_t raw_x)
{
    return (int16_t)(raw_x < 256U ? raw_x : (int)raw_x - 512);
}

bool mipsnes2_ppu_obj_select_scanline(const mipsnes2_ppu_oam_ports *ports,
                                     uint8_t line_y, unsigned first_sprite,
                                     mipsnes2_ppu_obj_scanline *output)
{
    mipsnes2_ppu_obj_scanline selected;
    uint8_t size;
    unsigned offset;
    unsigned count = 0U;

    if (ports == NULL || !ports->initialized || ports->oam == NULL ||
        first_sprite >= 128U || output == NULL ||
        !mipsnes2_ppu_oam_sprite_size(ports->obj_select, false, &size)) {
        return false;
    }
    (void)memset(&selected, 0, sizeof(selected));
    for (offset = 0U; offset < 128U; ++offset) {
        mipsnes2_ppu_oam_sprite sprite;
        unsigned index = (first_sprite + offset) & 127U;
        int16_t x;

        (void)mipsnes2_ppu_oam_decode_sprite(ports, index, &sprite);
        (void)mipsnes2_ppu_oam_sprite_size(ports->obj_select,
                                           sprite.large, &size);
        if (((uint8_t)(line_y - sprite.y)) >= size) {
            continue;
        }
        x = signed_x(sprite.x);
        if (sprite.x != 256U && x <= -(int)size) {
            continue;
        }
        if (count < MIPSNES2_PPU_OBJ_RANGE_LIMIT) {
            selected.range[count] = (uint8_t)index;
        } else {
            selected.range_over = true;
        }
        ++count;
    }
    selected.range_count = (uint8_t)(count < MIPSNES2_PPU_OBJ_RANGE_LIMIT ?
                                     count : MIPSNES2_PPU_OBJ_RANGE_LIMIT);

    /* The tile-fetch phase walks the selected sprites in reverse order. */
    for (offset = selected.range_count; offset != 0U; --offset) {
        mipsnes2_ppu_oam_sprite sprite;
        unsigned index = selected.range[offset - 1U];
        unsigned columns;
        unsigned row;
        unsigned column;
        int16_t x;

        (void)mipsnes2_ppu_oam_decode_sprite(ports, index, &sprite);
        (void)mipsnes2_ppu_oam_sprite_size(ports->obj_select,
                                           sprite.large, &size);
        columns = (unsigned)size / 8U;
        row = (unsigned)(uint8_t)(line_y - sprite.y) / 8U;
        x = signed_x(sprite.x);
        for (column = 0U; column < columns; ++column) {
            int tile_x = (int)x + (int)(column * 8U);
            mipsnes2_ppu_obj_sliver *sliver;

            if (sprite.x != 256U && (tile_x <= -8 || tile_x >= 256)) {
                continue;
            }
            if (selected.sliver_count >= MIPSNES2_PPU_OBJ_TIME_LIMIT) {
                selected.time_over = true;
                continue;
            }
            sliver = &selected.slivers[selected.sliver_count++];
            sliver->sprite_index = (uint8_t)index;
            sliver->source_tile_x = (uint8_t)(sprite.flip_x ?
                                             columns - 1U - column : column);
            sliver->source_tile_y = (uint8_t)(sprite.flip_y ?
                                             columns - 1U - row : row);
            sliver->screen_x = (int16_t)tile_x;
        }
    }
    *output = selected;
    return true;
}
