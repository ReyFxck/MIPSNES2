/* Host regression for the PS2 standard-pad mapping. See LICENSE. */
#include <stdint.h>
#include <stdio.h>

#include "core/bus/cpu_io.h"
#include "platform/ps2/pad_mapping.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

typedef struct {
    uint16_t ps2;
    uint16_t snes;
} mapping_case;

int main(void)
{
    static const mapping_case cases[] = {
        {MIPSNES2_PS2_PAD_CROSS, MIPSNES2_JOYPAD_B},
        {MIPSNES2_PS2_PAD_SQUARE, MIPSNES2_JOYPAD_Y},
        {MIPSNES2_PS2_PAD_SELECT, MIPSNES2_JOYPAD_SELECT},
        {MIPSNES2_PS2_PAD_START, MIPSNES2_JOYPAD_START},
        {MIPSNES2_PS2_PAD_UP, MIPSNES2_JOYPAD_UP},
        {MIPSNES2_PS2_PAD_DOWN, MIPSNES2_JOYPAD_DOWN},
        {MIPSNES2_PS2_PAD_LEFT, MIPSNES2_JOYPAD_LEFT},
        {MIPSNES2_PS2_PAD_RIGHT, MIPSNES2_JOYPAD_RIGHT},
        {MIPSNES2_PS2_PAD_CIRCLE, MIPSNES2_JOYPAD_A},
        {MIPSNES2_PS2_PAD_TRIANGLE, MIPSNES2_JOYPAD_X},
        {MIPSNES2_PS2_PAD_L1, MIPSNES2_JOYPAD_L},
        {MIPSNES2_PS2_PAD_R1, MIPSNES2_JOYPAD_R}
    };
    uint16_t mapped = 0U;
    size_t index;

    CHECK(mipsnes2_ps2_pad_translate(UINT16_MAX) == 0U);
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint16_t active_low = (uint16_t)~cases[index].ps2;

        CHECK(mipsnes2_ps2_pad_translate(active_low) == cases[index].snes);
        mapped |= cases[index].snes;
    }
    CHECK(mapped == UINT16_C(0xfff0));
    CHECK(mipsnes2_ps2_pad_translate(0U) == UINT16_C(0xfff0));
    CHECK(mipsnes2_ps2_pad_translate((uint16_t)~(
              MIPSNES2_PS2_PAD_L2 | MIPSNES2_PS2_PAD_R2 |
              MIPSNES2_PS2_PAD_L3 | MIPSNES2_PS2_PAD_R3)) == 0U);
    CHECK(mipsnes2_ps2_pad_translate((uint16_t)~(
              MIPSNES2_PS2_PAD_CROSS | MIPSNES2_PS2_PAD_RIGHT |
              MIPSNES2_PS2_PAD_R1)) ==
          (MIPSNES2_JOYPAD_B | MIPSNES2_JOYPAD_RIGHT |
           MIPSNES2_JOYPAD_R));
    (void)puts("PS2 standard-pad mapping: OK");
    return 0;
}
