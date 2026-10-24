/* Synthetic BG register-state fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>

#include "core/ppu/bg_registers.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

int main(void)
{
    mipsnes2_ppu_bg_registers registers;
    mipsnes2_ppu_bg2_config config;

    mipsnes2_ppu_bg_registers_init(&registers);
    CHECK(mipsnes2_ppu_bg_mode0_config(&registers, 0U, &config));
    CHECK(config.screen_base_word == 0U && config.tile_base_word == 0U &&
          config.scroll_x == 0U && config.palette_base == 0U &&
          !config.tile_size16);
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x2105U, 0x10U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x2107U, 0xfdU));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210bU, 0xf3U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210dU, 0x23U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210dU, 0x01U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210eU, 0x45U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210eU, 0x02U));
    CHECK(mipsnes2_ppu_bg_mode0_config(&registers, 0U, &config));
    CHECK(config.screen_base_word == UINT16_C(0x7c00) &&
          config.screen_size == 1U &&
          config.tile_base_word == UINT16_C(0x3000) &&
          config.scroll_x == UINT16_C(0x123) &&
          config.scroll_y == UINT16_C(0x245) &&
          config.tile_size16);
    CHECK(mipsnes2_ppu_bg_mode0_config(&registers, 1U, &config));
    CHECK(config.tile_base_word == UINT16_C(0x7000) &&
          config.palette_base == 32U && !config.tile_size16);
    CHECK(mipsnes2_ppu_bg_mode0_config(&registers, 3U, &config));
    CHECK(config.palette_base == 96U);
    /* A mixed write shows the shared previous-byte latches explicitly. */
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210fU, 0xa0U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210eU, 0x55U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x210fU, 1U));
    CHECK(registers.hofs[1] == UINT16_C(0x150));
    CHECK(!mipsnes2_ppu_bg_mode0_config(&registers, 4U, &config));
    CHECK(!mipsnes2_ppu_bg_registers_write(&registers, 0x2106U, 1U));
    CHECK(mipsnes2_ppu_bg_registers_write(&registers, 0x2105U, 1U));
    CHECK(!mipsnes2_ppu_bg_mode0_config(&registers, 0U, &config));
    CHECK(!mipsnes2_ppu_bg_registers_write(NULL, 0x2105U, 0U));
    (void)puts("PPU BG register subset: OK");
    return 0;
}
