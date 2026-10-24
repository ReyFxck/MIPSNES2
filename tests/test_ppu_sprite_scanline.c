/* Deterministic OBJ scanline fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/sprite_scanline.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t oam[MIPSNES2_PPU_OAM_SIZE];

static void set_sprite(unsigned index, unsigned x, uint8_t y,
                       bool large, uint8_t attributes)
{
    unsigned shift = (index & 3U) * 2U;
    uint8_t *high = &oam[512U + index / 4U];

    oam[index * 4U] = (uint8_t)x;
    oam[index * 4U + 1U] = y;
    oam[index * 4U + 3U] = attributes;
    *high = (uint8_t)((*high & ~(3U << shift)) |
                      ((((x >> 8) & 1U) | (large ? 2U : 0U)) << shift));
}

static void hide_all(void)
{
    unsigned index;

    (void)memset(oam, 0, sizeof(oam));
    for (index = 0U; index < 128U; ++index) {
        set_sprite(index, 257U, 10U, false, 0U);
    }
}

int main(void)
{
    mipsnes2_ppu_oam_ports ports;
    mipsnes2_ppu_obj_scanline line;
    unsigned index;

    hide_all();
    CHECK(mipsnes2_ppu_oam_ports_init(&ports, oam, sizeof(oam)));
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.range_count == 0U && line.sliver_count == 0U);
    CHECK(!line.range_over && !line.time_over);

    for (index = 0U; index < 32U; ++index) {
        set_sprite(index, 0U, 10U, false, 0U);
    }
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.range_count == 32U && !line.range_over);
    set_sprite(32U, 0U, 10U, false, 0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.range_count == 32U && line.range_over);
    CHECK(line.range[0] == 0U && line.range[31] == 31U);
    CHECK(line.sliver_count == 32U && !line.time_over);
    CHECK(line.slivers[0].sprite_index == 31U);
    CHECK(line.slivers[31].sprite_index == 0U);

    hide_all();
    for (index = 120U; index < 128U; ++index) {
        set_sprite(index, 0U, 10U, false, 0U);
    }
    for (index = 0U; index < 25U; ++index) {
        set_sprite(index, 0U, 10U, false, 0U);
    }
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 120U, &line));
    CHECK(line.range_count == 32U && line.range_over);
    CHECK(line.range[0] == 120U && line.range[7] == 127U);
    CHECK(line.range[8] == 0U && line.range[31] == 23U);

    hide_all();
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2101U, 0x40U));
    for (index = 0U; index < 5U; ++index) {
        set_sprite(index, 0U, 10U, true, 0U);
    }
    set_sprite(4U, 0U, 10U, true, 0xc0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.range_count == 5U && !line.range_over);
    CHECK(line.sliver_count == 34U && line.time_over);
    CHECK(line.slivers[0].sprite_index == 4U);
    CHECK(line.slivers[0].source_tile_x == 7U);
    CHECK(line.slivers[0].source_tile_y == 7U);
    CHECK(line.slivers[33].sprite_index == 0U);
    CHECK(line.slivers[33].source_tile_x == 1U);

    hide_all();
    for (index = 0U; index < 4U; ++index) {
        set_sprite(index, 0U, 10U, true, 0U);
    }
    set_sprite(4U, 0U, 10U, false, 0U);
    set_sprite(5U, 0U, 10U, false, 0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.sliver_count == 34U && !line.time_over);
    set_sprite(6U, 0U, 10U, false, 0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.sliver_count == 34U && line.time_over);

    hide_all();
    set_sprite(0U, 256U, 10U, true, 0U);
    set_sprite(1U, 257U, 10U, true, 0U);
    set_sprite(2U, 448U, 10U, true, 0U); /* X=-64, outside. */
    set_sprite(3U, 449U, 10U, true, 0U); /* X=-63, one tile. */
    set_sprite(4U, 255U, 10U, true, 0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(line.range_count == 3U && !line.range_over);
    CHECK(line.range[0] == 0U && line.range[1] == 3U && line.range[2] == 4U);
    CHECK(line.sliver_count == 10U && !line.time_over);
    CHECK(line.slivers[0].screen_x == 255);
    CHECK(line.slivers[1].screen_x == -7);
    CHECK(line.slivers[2].screen_x == -256);
    CHECK(line.slivers[9].screen_x == -200);

    hide_all();
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2101U, 0U));
    set_sprite(0U, 0U, 252U, false, 0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 3U, 0U, &line));
    CHECK(line.range_count == 1U && line.slivers[0].source_tile_y == 0U);
    CHECK(mipsnes2_ppu_obj_select_scanline(&ports, 4U, 0U, &line));
    CHECK(line.range_count == 0U);

    (void)memset(&line, 0x5a, sizeof(line));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2101U, 0xc0U));
    CHECK(!mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, &line));
    CHECK(!mipsnes2_ppu_obj_select_scanline(&ports, 10U, 128U, &line));
    CHECK(!mipsnes2_ppu_obj_select_scanline(&ports, 10U, 0U, NULL));
    CHECK(!mipsnes2_ppu_obj_select_scanline(NULL, 10U, 0U, &line));
    CHECK(line.range[0] == 0x5aU && line.sliver_count == 0x5aU);
    (void)puts("PPU OBJ scanline selection: OK");
    return 0;
}
