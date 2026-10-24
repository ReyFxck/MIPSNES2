/* MIPSNES2 BG register subset. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/bg_registers.h"

#include <string.h>

void mipsnes2_ppu_bg_registers_init(mipsnes2_ppu_bg_registers *registers)
{
    if (registers == NULL) {
        return;
    }
    (void)memset(registers, 0, sizeof(*registers));
    registers->initialized = true;
}

bool mipsnes2_ppu_bg_registers_write(mipsnes2_ppu_bg_registers *registers,
                                    uint16_t offset, uint8_t value)
{
    unsigned bg;

    if (registers == NULL || !registers->initialized) {
        return false;
    }
    if (offset == UINT16_C(0x2105)) {
        registers->mode = value;
    } else if (offset >= UINT16_C(0x2107) && offset <= UINT16_C(0x210a)) {
        registers->screen[offset - UINT16_C(0x2107)] = value;
    } else if (offset >= UINT16_C(0x210b) && offset <= UINT16_C(0x210c)) {
        registers->tile_bases[offset - UINT16_C(0x210b)] = value;
    } else if (offset >= UINT16_C(0x210d) && offset <= UINT16_C(0x2114)) {
        bg = (unsigned)(offset - UINT16_C(0x210d)) / 2U;
        if (((offset - UINT16_C(0x210d)) & 1U) == 0U) {
            registers->hofs[bg] = (uint16_t)(
                (((unsigned)value << 8) |
                 ((unsigned)registers->previous & ~7U) |
                 ((unsigned)registers->previous_horizontal & 7U)) & 0x3ffU);
            registers->previous_horizontal = value;
        } else {
            registers->vofs[bg] = (uint16_t)(
                (((unsigned)value << 8) | registers->previous) & 0x3ffU);
        }
        registers->previous = value;
    } else {
        return false;
    }
    return true;
}

bool mipsnes2_ppu_bg_mode0_config(const mipsnes2_ppu_bg_registers *registers,
                                 unsigned bg, mipsnes2_ppu_bg2_config *output)
{
    mipsnes2_ppu_bg2_config config;
    unsigned nibble;

    if (registers == NULL || !registers->initialized || bg >= 4U ||
        output == NULL || (registers->mode & 7U) != 0U) {
        return false;
    }
    nibble = (unsigned)registers->tile_bases[bg / 2U] >>
             ((bg & 1U) * 4U);
    config.screen_base_word =
        (uint16_t)((((unsigned)registers->screen[bg] & 0xfcU) << 8) &
                   0x7fffU);
    config.tile_base_word = (uint16_t)(((nibble & 15U) << 12) & 0x7fffU);
    config.scroll_x = registers->hofs[bg];
    config.scroll_y = registers->vofs[bg];
    config.screen_size = registers->screen[bg] & 3U;
    config.palette_base = (uint8_t)(bg * 32U);
    config.tile_size16 = (registers->mode & (UINT8_C(0x10) << bg)) != 0U;
    *output = config;
    return true;
}

bool mipsnes2_ppu_bg_mode1_config(const mipsnes2_ppu_bg_registers *registers,
                                 unsigned bg, mipsnes2_ppu_bg2_config *output)
{
    mipsnes2_ppu_bg2_config config;
    unsigned nibble;

    if (registers == NULL || !registers->initialized || bg >= 3U ||
        output == NULL || (registers->mode & 7U) != 1U) {
        return false;
    }
    nibble = (unsigned)registers->tile_bases[bg / 2U] >>
             ((bg & 1U) * 4U);
    config.screen_base_word =
        (uint16_t)((((unsigned)registers->screen[bg] & 0xfcU) << 8) &
                   0x7fffU);
    config.tile_base_word = (uint16_t)(((nibble & 15U) << 12) & 0x7fffU);
    config.scroll_x = registers->hofs[bg];
    config.scroll_y = registers->vofs[bg];
    config.screen_size = registers->screen[bg] & 3U;
    config.palette_base = 0U;
    config.tile_size16 = (registers->mode & (UINT8_C(0x10) << bg)) != 0U;
    *output = config;
    return true;
}
