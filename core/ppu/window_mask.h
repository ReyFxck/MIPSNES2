/* MIPSNES2 PPU layer windows. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_WINDOW_MASK_H
#define MIPSNES2_PPU_WINDOW_MASK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t select[3]; /* $2123-$2125: BG1-4, OBJ, color. */
    uint8_t edge[4];   /* $2126-$2129: left/right for windows 1 and 2. */
    uint8_t logic[2];  /* $212A-$212B: BG1-4, OBJ/color. */
    uint8_t main_mask; /* $212E: enable layer masking on main screen. */
    uint8_t sub_mask;  /* $212F: enable layer masking on sub screen. */
} mipsnes2_ppu_window_mask;

void mipsnes2_ppu_window_mask_init(mipsnes2_ppu_window_mask *state);
bool mipsnes2_ppu_window_mask_write(mipsnes2_ppu_window_mask *state,
                                    uint16_t offset, uint8_t value);
/* Main-screen output bit n hides BG(n+1), bit 4 hides OBJ. Inclusive edges;
 * left > right denotes an empty region before inversion. The color window
 * and sub-screen are deliberately outside this main-layer-mask subset.
 */
bool mipsnes2_ppu_window_mask_line(const mipsnes2_ppu_window_mask *state,
                                  uint8_t hidden[256]);
/* Same layer rules, independently gated by $212F. */
bool mipsnes2_ppu_window_mask_sub_line(const mipsnes2_ppu_window_mask *state,
                                      uint8_t hidden[256]);

#endif
