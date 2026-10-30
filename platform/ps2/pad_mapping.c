/* MIPSNES2 PS2-to-SNES standard-pad mapping. See LICENSE. */
#include "platform/ps2/pad_mapping.h"

#include "core/bus/cpu_io.h"

uint16_t mipsnes2_ps2_pad_translate(uint16_t active_low_buttons)
{
    uint16_t pressed = (uint16_t)~active_low_buttons;
    uint16_t result = 0U;

    if ((pressed & MIPSNES2_PS2_PAD_CROSS) != 0U) {
        result |= MIPSNES2_JOYPAD_B;
    }
    if ((pressed & MIPSNES2_PS2_PAD_SQUARE) != 0U) {
        result |= MIPSNES2_JOYPAD_Y;
    }
    if ((pressed & MIPSNES2_PS2_PAD_SELECT) != 0U) {
        result |= MIPSNES2_JOYPAD_SELECT;
    }
    if ((pressed & MIPSNES2_PS2_PAD_START) != 0U) {
        result |= MIPSNES2_JOYPAD_START;
    }
    if ((pressed & MIPSNES2_PS2_PAD_UP) != 0U) {
        result |= MIPSNES2_JOYPAD_UP;
    }
    if ((pressed & MIPSNES2_PS2_PAD_DOWN) != 0U) {
        result |= MIPSNES2_JOYPAD_DOWN;
    }
    if ((pressed & MIPSNES2_PS2_PAD_LEFT) != 0U) {
        result |= MIPSNES2_JOYPAD_LEFT;
    }
    if ((pressed & MIPSNES2_PS2_PAD_RIGHT) != 0U) {
        result |= MIPSNES2_JOYPAD_RIGHT;
    }
    if ((pressed & MIPSNES2_PS2_PAD_CIRCLE) != 0U) {
        result |= MIPSNES2_JOYPAD_A;
    }
    if ((pressed & MIPSNES2_PS2_PAD_TRIANGLE) != 0U) {
        result |= MIPSNES2_JOYPAD_X;
    }
    if ((pressed & MIPSNES2_PS2_PAD_L1) != 0U) {
        result |= MIPSNES2_JOYPAD_L;
    }
    if ((pressed & MIPSNES2_PS2_PAD_R1) != 0U) {
        result |= MIPSNES2_JOYPAD_R;
    }
    return result;
}
