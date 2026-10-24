/* MIPSNES2 SNES 4bpp tile decoding. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_TILE4_H
#define MIPSNES2_PPU_TILE4_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MIPSNES2_PPU_TILE4_PIXELS = 64, MIPSNES2_PPU_TILE4_VRAM_SIZE = 65536 };

/* Decode one 8x8 tile from 32K-word VRAM into row-major color indices 0..15.
 * The word address and tile fetch wrap in VRAM. Color 0 is left as index 0;
 * transparency, palette selection, flips and composition belong to callers.
 * Input and output buffers must not overlap. Output is untouched on invalid
 * input.
 */
bool mipsnes2_ppu_decode_4bpp_tile(const uint8_t *vram, size_t vram_size,
                                   uint16_t word_address,
                                   uint8_t pixels[MIPSNES2_PPU_TILE4_PIXELS]);

#endif
