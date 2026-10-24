/* Synthetic main-screen window vectors. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/window_mask.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t hidden[256];

int main(void)
{
    mipsnes2_ppu_window_mask state;
    unsigned logic;
    static const bool expected[4][4] = {
        { false, true, true, true },  /* OR */
        { false, false, false, true },/* AND */
        { false, true, true, false }, /* XOR */
        { true, false, false, true }  /* XNOR */
    };

    mipsnes2_ppu_window_mask_init(&state);
    (void)memset(hidden, UINT8_C(0xa5), sizeof(hidden));
    CHECK(mipsnes2_ppu_window_mask_line(&state, hidden));
    CHECK(hidden[0] == 0U && hidden[255] == 0U);
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2126), 10U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2127), 20U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2128), 15U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2129), 25U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2123), 10U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x212e), 1U));
    for (logic = 0U; logic < 4U; ++logic) {
        CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x212a),
                                              (uint8_t)logic));
        CHECK(mipsnes2_ppu_window_mask_line(&state, hidden));
        CHECK(((hidden[9] & 1U) != 0U) == expected[logic][0]);
        CHECK(((hidden[10] & 1U) != 0U) == expected[logic][1]);
        CHECK(((hidden[20] & 1U) != 0U) == expected[logic][3]);
        CHECK(((hidden[25] & 1U) != 0U) == expected[logic][2]);
        CHECK(((hidden[26] & 1U) != 0U) == expected[logic][0]);
    }
    /* Inversion and one-window mode ignore the unused logic setting. */
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2123), 3U));
    CHECK(mipsnes2_ppu_window_mask_line(&state, hidden));
    CHECK((hidden[9] & 1U) != 0U && (hidden[10] & 1U) == 0U);
    CHECK((hidden[20] & 1U) == 0U && (hidden[21] & 1U) != 0U);
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2123), 0U));
    CHECK(mipsnes2_ppu_window_mask_line(&state, hidden));
    CHECK(hidden[9] == 0U && hidden[10] == 0U);
    /* Independent BG4 and OBJ nibbles; TMW gates both. */
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2124),
                                          UINT8_C(0x20)));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2125), 8U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x212e),
                                          UINT8_C(0x18)));
    CHECK(mipsnes2_ppu_window_mask_line(&state, hidden));
    CHECK(hidden[10] == 8U && hidden[25] == 16U);
    CHECK(hidden[15] == 24U && hidden[9] == 0U);
    /* Reversed edges define an empty window before inversion. */
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2126), 30U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2127), 20U));
    CHECK(mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x2124),
                                          UINT8_C(0x30)));
    CHECK(mipsnes2_ppu_window_mask_line(&state, hidden));
    CHECK(hidden[0] == 8U && hidden[15] == 24U);
    CHECK(!mipsnes2_ppu_window_mask_write(&state, UINT16_C(0x212f), 31U));
    CHECK(!mipsnes2_ppu_window_mask_line(NULL, hidden));
    CHECK(!mipsnes2_ppu_window_mask_line(&state, NULL));
    (void)puts("PPU main-screen windows: OK");
    return 0;
}
