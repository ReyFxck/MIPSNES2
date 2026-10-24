/* MIPSNES2 PPU OAM CPU ports. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PPU_OAM_PORTS_H
#define MIPSNES2_PPU_OAM_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MIPSNES2_PPU_OAM_SIZE = 512 + 32 };

typedef struct {
    uint8_t *oam;
    uint8_t obj_select; /* $2101: size pair, name selection and base. */
    uint16_t word_address; /* $2102 plus bit 0 of $2103. */
    uint16_t byte_address; /* Ten-bit internal byte pointer. */
    uint8_t low_latch;
    bool priority_rotation;
    bool initialized;
} mipsnes2_ppu_oam_ports;

/* One decoded OAM record. X is the raw nine-bit coordinate (0..511);
 * display wrapping and scanline eligibility belong to sprite evaluation.
 * Character bit 8 selects the second OBJ name table.
 */
typedef struct {
    uint16_t x;
    uint8_t y;
    uint16_t character;
    uint8_t palette;
    uint8_t priority;
    bool flip_x;
    bool flip_y;
    bool large;
} mipsnes2_ppu_oam_sprite;

/* Caller owns 544 bytes; port reset retains the OAM bytes. */
bool mipsnes2_ppu_oam_ports_init(mipsnes2_ppu_oam_ports *ports,
                                uint8_t *oam, size_t size);
bool mipsnes2_ppu_oam_ports_write(mipsnes2_ppu_oam_ports *ports,
                                 uint16_t offset, uint8_t value);
bool mipsnes2_ppu_oam_ports_read(mipsnes2_ppu_oam_ports *ports,
                                uint16_t offset, uint8_t *value);
/* At VBlank, the internal pointer reloads from the saved address only while
 * display is enabled. The write latch and physical OAM bytes are retained.
 */
bool mipsnes2_ppu_oam_ports_vblank_reload(mipsnes2_ppu_oam_ports *ports,
                                         bool forced_blank);

/* Read the current physical OAM bytes without changing CPU port state. */
bool mipsnes2_ppu_oam_decode_sprite(const mipsnes2_ppu_oam_ports *ports,
                                   unsigned index,
                                   mipsnes2_ppu_oam_sprite *sprite);
/* Returns false for undocumented $2101 size codes 6 and 7. */
bool mipsnes2_ppu_oam_sprite_size(uint8_t obj_select, bool large,
                                 uint8_t *pixels);

#endif
