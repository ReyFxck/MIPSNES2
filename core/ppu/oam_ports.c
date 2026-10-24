/* MIPSNES2 PPU OAM CPU ports. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/oam_ports.h"

#include <string.h>

static void reload_address(mipsnes2_ppu_oam_ports *ports)
{
    ports->byte_address = (uint16_t)(ports->word_address << 1);
}

static size_t physical_index(uint16_t byte_address)
{
    if ((byte_address & UINT16_C(0x200)) == 0U) {
        return (size_t)(byte_address & UINT16_C(0x1ff));
    }
    return 512U + (size_t)(byte_address & UINT16_C(0x1f));
}

static void advance_address(mipsnes2_ppu_oam_ports *ports)
{
    ports->byte_address = (uint16_t)((ports->byte_address + 1U) &
                                     UINT16_C(0x3ff));
}

bool mipsnes2_ppu_oam_ports_init(mipsnes2_ppu_oam_ports *ports,
                                uint8_t *oam, size_t size)
{
    if (ports == NULL || oam == NULL || size < MIPSNES2_PPU_OAM_SIZE) {
        return false;
    }
    (void)memset(ports, 0, sizeof(*ports));
    ports->oam = oam;
    ports->initialized = true;
    return true;
}

bool mipsnes2_ppu_oam_ports_write(mipsnes2_ppu_oam_ports *ports,
                                 uint16_t offset, uint8_t value)
{
    size_t index;

    if (ports == NULL || !ports->initialized) {
        return false;
    }
    if (offset == UINT16_C(0x2101)) {
        ports->obj_select = value;
        return true;
    }
    if (offset == UINT16_C(0x2102)) {
        ports->word_address = (uint16_t)((ports->word_address &
                              UINT16_C(0x100)) | value);
        reload_address(ports);
        return true;
    }
    if (offset == UINT16_C(0x2103)) {
        ports->word_address = (uint16_t)((ports->word_address &
                              UINT16_C(0xff)) | ((uint16_t)(value & 1U) << 8));
        ports->priority_rotation = (value & UINT8_C(0x80)) != 0U;
        reload_address(ports);
        return true;
    }
    if (offset != UINT16_C(0x2104)) {
        return false;
    }
    index = physical_index(ports->byte_address);
    if ((ports->byte_address & UINT16_C(0x200)) != 0U) {
        ports->oam[index] = value;
    } else if ((ports->byte_address & 1U) == 0U) {
        ports->low_latch = value;
    } else {
        ports->oam[index - 1U] = ports->low_latch;
        ports->oam[index] = value;
    }
    advance_address(ports);
    return true;
}

bool mipsnes2_ppu_oam_ports_read(mipsnes2_ppu_oam_ports *ports,
                                uint16_t offset, uint8_t *value)
{
    if (ports == NULL || !ports->initialized || value == NULL ||
        offset != UINT16_C(0x2138)) {
        return false;
    }
    *value = ports->oam[physical_index(ports->byte_address)];
    advance_address(ports);
    return true;
}

bool mipsnes2_ppu_oam_ports_vblank_reload(mipsnes2_ppu_oam_ports *ports,
                                         bool forced_blank)
{
    if (ports == NULL || !ports->initialized) {
        return false;
    }
    if (!forced_blank) {
        reload_address(ports);
    }
    return true;
}

bool mipsnes2_ppu_oam_decode_sprite(const mipsnes2_ppu_oam_ports *ports,
                                   unsigned index,
                                   mipsnes2_ppu_oam_sprite *sprite)
{
    size_t base;
    uint8_t high;
    uint8_t attributes;

    if (ports == NULL || !ports->initialized || ports->oam == NULL ||
        index >= 128U || sprite == NULL) {
        return false;
    }
    base = (size_t)index * 4U;
    high = (uint8_t)(ports->oam[512U + index / 4U] >>
                     (2U * (index & 3U)));
    attributes = ports->oam[base + 3U];
    sprite->x = (uint16_t)(ports->oam[base] | ((uint16_t)(high & 1U) << 8));
    sprite->y = ports->oam[base + 1U];
    sprite->character = (uint16_t)(ports->oam[base + 2U] |
                                   ((uint16_t)(attributes & 1U) << 8));
    sprite->palette = (uint8_t)((attributes >> 1) & 7U);
    sprite->priority = (uint8_t)((attributes >> 4) & 3U);
    sprite->flip_x = (attributes & UINT8_C(0x40)) != 0U;
    sprite->flip_y = (attributes & UINT8_C(0x80)) != 0U;
    sprite->large = (high & 2U) != 0U;
    return true;
}

bool mipsnes2_ppu_oam_sprite_size(uint8_t obj_select, bool large,
                                 uint8_t *pixels)
{
    static const uint8_t sizes[6][2] = {
        { 8U, 16U }, { 8U, 32U }, { 8U, 64U },
        { 16U, 32U }, { 16U, 64U }, { 32U, 64U }
    };
    unsigned mode = (unsigned)(obj_select >> 5);

    if (pixels == NULL || mode >= 6U) {
        return false;
    }
    *pixels = sizes[mode][large ? 1U : 0U];
    return true;
}
