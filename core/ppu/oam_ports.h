/* MIPSNES2 PPU OAM CPU ports. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_OAM_PORTS_H
#define MIPSNES2_PPU_OAM_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MIPSNES2_PPU_OAM_SIZE = 512 + 32 };

typedef struct {
    uint8_t *oam;
    uint16_t word_address; /* $2102 plus bit 0 of $2103. */
    uint16_t byte_address; /* Ten-bit internal byte pointer. */
    uint8_t low_latch;
    bool priority_rotation;
    bool initialized;
} mipsnes2_ppu_oam_ports;

/* Caller owns 544 bytes; port reset retains the OAM bytes. */
bool mipsnes2_ppu_oam_ports_init(mipsnes2_ppu_oam_ports *ports,
                                uint8_t *oam, size_t size);
bool mipsnes2_ppu_oam_ports_write(mipsnes2_ppu_oam_ports *ports,
                                 uint16_t offset, uint8_t value);
bool mipsnes2_ppu_oam_ports_read(mipsnes2_ppu_oam_ports *ports,
                                uint16_t offset, uint8_t *value);

#endif
