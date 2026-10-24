/* MIPSNES2 BG register subset. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_BG_REGISTERS_H
#define MIPSNES2_PPU_BG_REGISTERS_H

#include "core/ppu/bg2_scanline.h"

typedef struct {
    uint8_t mode;
    uint8_t screen[4];
    uint8_t tile_bases[2];
    uint16_t hofs[4];
    uint16_t vofs[4];
    uint8_t previous;
    uint8_t previous_horizontal;
    bool initialized;
} mipsnes2_ppu_bg_registers;

void mipsnes2_ppu_bg_registers_init(mipsnes2_ppu_bg_registers *registers);
bool mipsnes2_ppu_bg_registers_write(mipsnes2_ppu_bg_registers *registers,
                                    uint16_t offset, uint8_t value);
/* Currently only Mode 0 has a 2bpp configuration for all four BGs. */
bool mipsnes2_ppu_bg_mode0_config(const mipsnes2_ppu_bg_registers *registers,
                                 unsigned bg, mipsnes2_ppu_bg2_config *output);

#endif
