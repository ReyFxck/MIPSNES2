/* MIPSNES2 base cartridge mapping foundation. See LICENSE for GPL-3.0 terms. */
#include "core/cartridge/cartridge.h"

#include <string.h>

static bool is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

static size_t largest_power_of_two(size_t value)
{
    size_t power = 1U;

    while (power <= value / 2U) {
        power *= 2U;
    }
    return power;
}

static bool analyze_rom_layout(size_t size,
                               size_t bank_size,
                               size_t *first_size,
                               size_t *mirror_span)
{
    size_t first;
    size_t tail;

    if (size < bank_size || size > MIPSNES2_BASE_ROM_MAX_SIZE ||
        size % bank_size != 0U) {
        return false;
    }
    first = largest_power_of_two(size);
    tail = size - first;
    if (tail != 0U && !is_power_of_two(tail)) {
        return false;
    }
    *first_size = first;
    *mirror_span = tail == 0U ? first : first * 2U;
    return true;
}

static bool is_system_bank(uint8_t bank)
{
    return bank <= UINT8_C(0x3f) ||
           (bank >= UINT8_C(0x80) && bank <= UINT8_C(0xbf));
}

static bool lorom_sram_offset(mipsnes2_address address, size_t *offset)
{
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;

    if (local >= UINT16_C(0x8000) ||
        !((bank >= UINT8_C(0x70) && bank <= UINT8_C(0x7d)) ||
          bank >= UINT8_C(0xf0))) {
        return false;
    }
    *offset = ((size_t)(bank & UINT8_C(0x0f)) << 15) |
              (size_t)local;
    return true;
}

static bool hirom_sram_offset(mipsnes2_address address, size_t *offset)
{
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;

    if (local < UINT16_C(0x6000) || local >= UINT16_C(0x8000) ||
        !((bank >= UINT8_C(0x20) && bank <= UINT8_C(0x3f)) ||
          (bank >= UINT8_C(0xa0) && bank <= UINT8_C(0xbf)))) {
        return false;
    }
    *offset = ((size_t)(bank & UINT8_C(0x1f)) << 13) |
              (size_t)(local & UINT16_C(0x1fff));
    return true;
}

static bool lorom_rom_offset(mipsnes2_address address, size_t *offset)
{
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;
    bool full_rom_bank = (bank >= UINT8_C(0x40) && bank <= UINT8_C(0x6f)) ||
                         (bank >= UINT8_C(0xc0) && bank <= UINT8_C(0xef));

    if (!full_rom_bank && local < UINT16_C(0x8000)) {
        return false;
    }
    if (bank == UINT8_C(0x7e) || bank == UINT8_C(0x7f)) {
        return false;
    }
    *offset = ((size_t)(bank & UINT8_C(0x7f)) << 15) |
              (size_t)(local & UINT16_C(0x7fff));
    return true;
}

static bool hirom_rom_offset(mipsnes2_address address, size_t *offset)
{
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;
    bool full_rom_bank = (bank >= UINT8_C(0x40) && bank <= UINT8_C(0x7d)) ||
                         bank >= UINT8_C(0xc0);

    if (!full_rom_bank && (!is_system_bank(bank) ||
                           local < UINT16_C(0x8000))) {
        return false;
    }
    *offset = ((size_t)(bank & UINT8_C(0x3f)) << 16) | (size_t)local;
    return true;
}

static bool sram_offset(const mipsnes2_cartridge *cartridge,
                        mipsnes2_address address,
                        size_t *offset)
{
    if (cartridge->sram_size == 0U) {
        return false;
    }
    if (cartridge->mapper == MIPSNES2_CARTRIDGE_LOROM) {
        return lorom_sram_offset(address, offset);
    }
    return hirom_sram_offset(address, offset);
}

static bool rom_offset(const mipsnes2_cartridge *cartridge,
                       mipsnes2_address address,
                       size_t *offset)
{
    if (cartridge->mapper == MIPSNES2_CARTRIDGE_LOROM) {
        return lorom_rom_offset(address, offset);
    }
    return hirom_rom_offset(address, offset);
}

static size_t mirror_rom_offset(const mipsnes2_cartridge *cartridge,
                                size_t offset)
{
    size_t tail_size;

    offset &= cartridge->rom_mirror_span - 1U;
    if (offset < cartridge->rom_first_size) {
        return offset;
    }
    tail_size = cartridge->rom_size - cartridge->rom_first_size;
    return cartridge->rom_first_size +
           ((offset - cartridge->rom_first_size) & (tail_size - 1U));
}

mipsnes2_cartridge_result mipsnes2_cartridge_init(
    mipsnes2_cartridge *cartridge,
    const mipsnes2_cartridge_config *config)
{
    size_t rom_min;
    size_t rom_first_size;
    size_t rom_mirror_span;
    size_t sram_max;

    if (cartridge == NULL || config == NULL || config->rom == NULL ||
        (config->sram_size != 0U && config->sram == NULL)) {
        return MIPSNES2_CARTRIDGE_INVALID_ARGUMENT;
    }
    if (config->mapper != MIPSNES2_CARTRIDGE_LOROM &&
        config->mapper != MIPSNES2_CARTRIDGE_HIROM) {
        return MIPSNES2_CARTRIDGE_UNSUPPORTED_MAPPER;
    }
    rom_min = config->mapper == MIPSNES2_CARTRIDGE_LOROM
                  ? MIPSNES2_LOROM_BANK_SIZE
                  : MIPSNES2_HIROM_BANK_SIZE;
    if (!analyze_rom_layout(config->rom_size, rom_min,
                            &rom_first_size, &rom_mirror_span)) {
        return MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE;
    }
    sram_max = config->mapper == MIPSNES2_CARTRIDGE_LOROM
                   ? MIPSNES2_LOROM_SRAM_MAX_SIZE
                   : MIPSNES2_HIROM_SRAM_MAX_SIZE;
    if (config->sram_size > sram_max ||
        (config->sram_size != 0U && !is_power_of_two(config->sram_size))) {
        return MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE;
    }

    (void)memset(cartridge, 0, sizeof(*cartridge));
    cartridge->rom = config->rom;
    cartridge->rom_size = config->rom_size;
    cartridge->rom_first_size = rom_first_size;
    cartridge->rom_mirror_span = rom_mirror_span;
    cartridge->sram = config->sram;
    cartridge->sram_size = config->sram_size;
    cartridge->mapper = config->mapper;
    return MIPSNES2_CARTRIDGE_OK;
}

bool mipsnes2_cartridge_read(mipsnes2_cartridge *cartridge,
                             mipsnes2_address address,
                             uint8_t *value)
{
    size_t offset;

    if (cartridge == NULL || value == NULL || cartridge->rom == NULL) {
        return false;
    }
    address = mipsnes2_address24(address);
    if (sram_offset(cartridge, address, &offset)) {
        *value = cartridge->sram[offset & (cartridge->sram_size - 1U)];
        return true;
    }
    if (!rom_offset(cartridge, address, &offset)) {
        return false;
    }
    *value = cartridge->rom[mirror_rom_offset(cartridge, offset)];
    return true;
}

bool mipsnes2_cartridge_write(mipsnes2_cartridge *cartridge,
                              mipsnes2_address address,
                              uint8_t value)
{
    size_t offset;

    if (cartridge == NULL || cartridge->rom == NULL) {
        return false;
    }
    address = mipsnes2_address24(address);
    if (sram_offset(cartridge, address, &offset)) {
        cartridge->sram[offset & (cartridge->sram_size - 1U)] = value;
        return true;
    }
    return rom_offset(cartridge, address, &offset);
}

static bool interface_read(void *context,
                           mipsnes2_address address,
                           uint8_t *value)
{
    return mipsnes2_cartridge_read((mipsnes2_cartridge *)context,
                                   address, value);
}

static bool interface_write(void *context,
                            mipsnes2_address address,
                            uint8_t value)
{
    return mipsnes2_cartridge_write((mipsnes2_cartridge *)context,
                                    address, value);
}

mipsnes2_cartridge_interface mipsnes2_cartridge_make_interface(
    mipsnes2_cartridge *cartridge)
{
    mipsnes2_cartridge_interface interface;

    interface.context = cartridge;
    interface.read8 = cartridge != NULL ? interface_read : NULL;
    interface.write8 = cartridge != NULL ? interface_write : NULL;
    return interface;
}
