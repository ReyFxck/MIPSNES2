/* MIPSNES2 main-screen layer windows. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/window_mask.h"

#include <string.h>

void mipsnes2_ppu_window_mask_init(mipsnes2_ppu_window_mask *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

bool mipsnes2_ppu_window_mask_write(mipsnes2_ppu_window_mask *state,
                                    uint16_t offset, uint8_t value)
{
    if (state == NULL) {
        return false;
    }
    if (offset >= UINT16_C(0x2123) && offset <= UINT16_C(0x2125)) {
        state->select[offset - UINT16_C(0x2123)] = value;
    } else if (offset >= UINT16_C(0x2126) && offset <= UINT16_C(0x2129)) {
        state->edge[offset - UINT16_C(0x2126)] = value;
    } else if (offset >= UINT16_C(0x212a) && offset <= UINT16_C(0x212b)) {
        state->logic[offset - UINT16_C(0x212a)] = value;
    } else if (offset == UINT16_C(0x212e)) {
        state->main_mask = value & UINT8_C(0x1f);
    } else {
        return false;
    }
    return true;
}

static bool window_inside(const mipsnes2_ppu_window_mask *state,
                          unsigned window, unsigned x)
{
    uint8_t left = state->edge[window * 2U];
    uint8_t right = state->edge[window * 2U + 1U];

    return left <= right && x >= left && x <= right;
}

static bool layer_hidden(const mipsnes2_ppu_window_mask *state,
                         unsigned layer, unsigned x)
{
    unsigned shift = layer == 4U ? 0U : (layer & 1U) * 4U;
    uint8_t select = state->select[layer == 4U ? 2U : layer / 2U];
    uint8_t nibble = (uint8_t)((select >> shift) & 15U);
    bool first_enabled = (nibble & 2U) != 0U;
    bool second_enabled = (nibble & 8U) != 0U;
    bool first;
    bool second;
    uint8_t logic;

    if (!first_enabled && !second_enabled) {
        return false;
    }
    first = window_inside(state, 0U, x) != ((nibble & 1U) != 0U);
    second = window_inside(state, 1U, x) != ((nibble & 4U) != 0U);
    if (!first_enabled) {
        return second;
    }
    if (!second_enabled) {
        return first;
    }
    logic = layer == 4U ? state->logic[1] & 3U :
        (state->logic[0] >> (layer * 2U)) & 3U;
    switch (logic) {
    case 0U: return first || second;
    case 1U: return first && second;
    case 2U: return first != second;
    default: return first == second;
    }
}

bool mipsnes2_ppu_window_mask_line(const mipsnes2_ppu_window_mask *state,
                                  uint8_t hidden[256])
{
    unsigned x;
    unsigned layer;

    if (state == NULL || hidden == NULL) {
        return false;
    }
    for (x = 0U; x < 256U; ++x) {
        uint8_t mask = 0U;

        for (layer = 0U; layer < 5U; ++layer) {
            if ((state->main_mask & (1U << layer)) != 0U &&
                layer_hidden(state, layer, x)) {
                mask |= (uint8_t)(1U << layer);
            }
        }
        hidden[x] = mask;
    }
    return true;
}
