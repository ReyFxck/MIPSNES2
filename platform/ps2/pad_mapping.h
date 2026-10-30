/* MIPSNES2 PS2-to-SNES standard-pad mapping. See LICENSE. */
#ifndef MIPSNES2_PS2_PAD_MAPPING_H
#define MIPSNES2_PS2_PAD_MAPPING_H

#include <stdint.h>

enum {
    MIPSNES2_PS2_PAD_SELECT = 0x0001,
    MIPSNES2_PS2_PAD_L3 = 0x0002,
    MIPSNES2_PS2_PAD_R3 = 0x0004,
    MIPSNES2_PS2_PAD_START = 0x0008,
    MIPSNES2_PS2_PAD_UP = 0x0010,
    MIPSNES2_PS2_PAD_RIGHT = 0x0020,
    MIPSNES2_PS2_PAD_DOWN = 0x0040,
    MIPSNES2_PS2_PAD_LEFT = 0x0080,
    MIPSNES2_PS2_PAD_L2 = 0x0100,
    MIPSNES2_PS2_PAD_R2 = 0x0200,
    MIPSNES2_PS2_PAD_L1 = 0x0400,
    MIPSNES2_PS2_PAD_R1 = 0x0800,
    MIPSNES2_PS2_PAD_TRIANGLE = 0x1000,
    MIPSNES2_PS2_PAD_CIRCLE = 0x2000,
    MIPSNES2_PS2_PAD_CROSS = 0x4000,
    MIPSNES2_PS2_PAD_SQUARE = 0x8000
};

/* libpad reports zero for pressed buttons. Analog axes and L2/R2/L3/R3 are
 * deliberately ignored by the standard SNES pad mapping. */
uint16_t mipsnes2_ps2_pad_translate(uint16_t active_low_buttons);

#endif
