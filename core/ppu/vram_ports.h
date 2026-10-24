/* MIPSNES2 PPU VRAM write-port foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_VRAM_PORTS_H
#define MIPSNES2_PPU_VRAM_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MIPSNES2_PPU_VRAM_SIZE = 64 * 1024 };

typedef struct {
    uint8_t *vram;
    uint16_t word_address;
    uint8_t vmain;
    bool initialized;
} mipsnes2_ppu_vram_ports;

/* Resets port registers, retaining the caller-owned VRAM bytes. */
bool mipsnes2_ppu_vram_ports_init(mipsnes2_ppu_vram_ports *ports,
                                 uint8_t *vram, size_t size);
/* Handles $2115-$2119 writes. Unknown registers return false for routing. */
bool mipsnes2_ppu_vram_ports_write(mipsnes2_ppu_vram_ports *ports,
                                  uint16_t offset, uint8_t value);

#endif
