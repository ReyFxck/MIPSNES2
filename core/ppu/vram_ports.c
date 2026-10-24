/* MIPSNES2 PPU VRAM port foundation. See LICENSE for GPL-3.0 terms. */
#include "core/ppu/vram_ports.h"

#include <string.h>

static uint16_t mapped_word(const mipsnes2_ppu_vram_ports *ports)
{
    uint32_t address = ports->word_address;
    unsigned mode = (ports->vmain >> 2) & 3U;

    if (mode != 0U) {
        unsigned bits = mode + 4U; /* 32, 64 or 128 words per group. */
        uint32_t group = UINT32_C(1) << bits;
        uint32_t span = group << 3;

        address = (address & ~(span - UINT32_C(1))) |
                  ((address & (group - UINT32_C(1))) << 3) |
                  ((address >> bits) & UINT32_C(7));
    }
    return (uint16_t)(address & UINT32_C(0x7fff));
}

static void increment_address(mipsnes2_ppu_vram_ports *ports)
{
    static const uint16_t increments[4] = {1U, 32U, 128U, 128U};

    ports->word_address = (uint16_t)(ports->word_address +
                                     increments[ports->vmain & 3U]);
}

bool mipsnes2_ppu_vram_ports_init(mipsnes2_ppu_vram_ports *ports,
                                 uint8_t *vram, size_t size)
{
    if (ports == NULL || vram == NULL || size < MIPSNES2_PPU_VRAM_SIZE) {
        return false;
    }
    (void)memset(ports, 0, sizeof(*ports));
    ports->vram = vram;
    ports->initialized = true;
    return true;
}

bool mipsnes2_ppu_vram_ports_write(mipsnes2_ppu_vram_ports *ports,
                                  uint16_t offset, uint8_t value)
{
    uint32_t index;

    if (ports == NULL || !ports->initialized) {
        return false;
    }
    switch (offset) {
    case 0x2115:
        ports->vmain = value;
        return true;
    case 0x2116:
        ports->word_address =
            (uint16_t)((ports->word_address & UINT16_C(0xff00)) | value);
        return true;
    case 0x2117:
        ports->word_address =
            (uint16_t)((ports->word_address & UINT16_C(0x00ff)) |
                       ((uint16_t)value << 8));
        return true;
    case 0x2118:
    case 0x2119:
        index = (uint32_t)mapped_word(ports) * 2U;
        if (offset == UINT16_C(0x2119)) {
            ++index;
        }
        ports->vram[index] = value;
        if (((ports->vmain & UINT8_C(0x80)) == 0U) ==
            (offset == UINT16_C(0x2118))) {
            increment_address(ports);
        }
        return true;
    default:
        return false;
    }
}

bool mipsnes2_ppu_vram_ports_read(mipsnes2_ppu_vram_ports *ports,
                                 uint16_t offset, uint8_t *value)
{
    uint32_t index;

    if (ports == NULL || !ports->initialized || value == NULL ||
        (offset != UINT16_C(0x2139) && offset != UINT16_C(0x213a))) {
        return false;
    }
    *value = offset == UINT16_C(0x2139)
                 ? (uint8_t)ports->read_buffer
                 : (uint8_t)(ports->read_buffer >> 8);
    if (((ports->vmain & UINT8_C(0x80)) == 0U) ==
        (offset == UINT16_C(0x2139))) {
        index = (uint32_t)mapped_word(ports) * 2U;
        ports->read_buffer = (uint16_t)(
            (uint16_t)ports->vram[index] |
            ((uint16_t)ports->vram[index + 1U] << 8));
        increment_address(ports);
    }
    return true;
}
