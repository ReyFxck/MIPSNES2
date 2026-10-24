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
