/* MIPSNES2 PPU CGRAM ports. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/cgram_ports.h"

#include <string.h>

bool mipsnes2_ppu_cgram_ports_init(mipsnes2_ppu_cgram_ports *ports,
                                  uint8_t *cgram, size_t size)
{
    if (ports == NULL || cgram == NULL || size < MIPSNES2_PPU_CGRAM_SIZE) {
        return false;
    }
    (void)memset(ports, 0, sizeof(*ports));
    ports->cgram = cgram;
    ports->initialized = true;
    return true;
}

bool mipsnes2_ppu_cgram_ports_write(mipsnes2_ppu_cgram_ports *ports,
                                   uint16_t offset, uint8_t value)
{
    size_t index;

    if (ports == NULL || !ports->initialized) {
        return false;
    }
    if (offset == UINT16_C(0x2121)) {
        ports->address = value;
        ports->high_phase = false;
        return true;
    }
    if (offset != UINT16_C(0x2122)) {
        return false;
    }
    if (!ports->high_phase) {
        ports->low_latch = value;
        ports->high_phase = true;
        return true;
    }
    index = (size_t)ports->address * 2U;
    ports->cgram[index] = ports->low_latch;
    ports->cgram[index + 1U] = value & UINT8_C(0x7f);
    ports->address = (uint8_t)(ports->address + 1U);
    ports->high_phase = false;
    return true;
}

bool mipsnes2_ppu_cgram_ports_read(mipsnes2_ppu_cgram_ports *ports,
                                  uint16_t offset, uint8_t ppu2_open_bus,
                                  uint8_t *value)
{
    size_t index;

    if (ports == NULL || !ports->initialized || value == NULL ||
        offset != UINT16_C(0x213b)) {
        return false;
    }
    index = (size_t)ports->address * 2U;
    if (!ports->high_phase) {
        *value = ports->cgram[index];
        ports->high_phase = true;
    } else {
        *value = (uint8_t)(ports->cgram[index + 1U] & UINT8_C(0x7f)) |
                 (uint8_t)(ppu2_open_bus & UINT8_C(0x80));
        ports->address = (uint8_t)(ports->address + 1U);
        ports->high_phase = false;
    }
    return true;
}
