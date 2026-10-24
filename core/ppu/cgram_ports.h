/* MIPSNES2 PPU CGRAM ports. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_CGRAM_PORTS_H
#define MIPSNES2_PPU_CGRAM_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MIPSNES2_PPU_CGRAM_SIZE = 256 * 2 };

typedef struct {
    uint8_t *cgram;
    uint8_t address;
    uint8_t low_latch;
    bool high_phase;
    bool initialized;
} mipsnes2_ppu_cgram_ports;

/* Caller owns 512 bytes; port reset retains the palette bytes. */
bool mipsnes2_ppu_cgram_ports_init(mipsnes2_ppu_cgram_ports *ports,
                                  uint8_t *cgram, size_t size);
/* $2121 sets the word address; pairs of $2122 writes commit 15-bit colors. */
bool mipsnes2_ppu_cgram_ports_write(mipsnes2_ppu_cgram_ports *ports,
                                   uint16_t offset, uint8_t value);
/* $213B alternates low/high; bit 7 of the high read is PPU2 open bus.
 * The caller owns that shared latch and updates it after a handled read.
 */
bool mipsnes2_ppu_cgram_ports_read(mipsnes2_ppu_cgram_ports *ports,
                                  uint16_t offset, uint8_t ppu2_open_bus,
                                  uint8_t *value);

#endif
