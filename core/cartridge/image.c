/* MIPSNES2 cartridge-image metadata foundation. See LICENSE for GPL-3.0 terms. */
#include "core/cartridge/image.h"

#include <limits.h>
#include <string.h>

static uint16_t read16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static bool decode_size(uint8_t exponent, bool zero_is_none, size_t *size)
{
    unsigned int size_bits = (unsigned int)(sizeof(size_t) * CHAR_BIT);

    if (zero_is_none && exponent == 0U) {
        *size = 0U;
        return true;
    }
    if ((unsigned int)exponent >= size_bits - 10U) {
        *size = 0U;
        return false;
    }
    *size = ((size_t)1U << exponent) * (size_t)1024U;
    return true;
}

static mipsnes2_cartridge_header_map classify_map(uint8_t map_mode)
{
    if (map_mode == UINT8_C(0x20) || map_mode == UINT8_C(0x30)) {
        return MIPSNES2_HEADER_MAP_BASE_LOROM;
    }
    if (map_mode == UINT8_C(0x21) || map_mode == UINT8_C(0x31)) {
        return MIPSNES2_HEADER_MAP_BASE_HIROM;
    }
    return MIPSNES2_HEADER_MAP_OTHER;
}

mipsnes2_cartridge_image_result mipsnes2_cartridge_image_view_init(
    mipsnes2_cartridge_image_view *view,
    const uint8_t *source,
    size_t source_size,
    mipsnes2_copier_header_policy policy)
{
    size_t remainder;
    bool has_copier_header;

    if (view == NULL || source == NULL) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    if (policy != MIPSNES2_COPIER_HEADER_DETECT &&
        policy != MIPSNES2_COPIER_HEADER_REJECT &&
        policy != MIPSNES2_COPIER_HEADER_REQUIRE) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_POLICY;
    }
    if (source_size == 0U) {
        return MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE;
    }

    remainder = source_size % (size_t)MIPSNES2_LOROM_BANK_SIZE;
    has_copier_header = remainder == (size_t)MIPSNES2_COPIER_HEADER_SIZE;
    if (remainder != 0U && !has_copier_header) {
        return MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE;
    }
    if (has_copier_header &&
        source_size <= (size_t)MIPSNES2_COPIER_HEADER_SIZE) {
        return MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE;
    }
    if (has_copier_header && policy == MIPSNES2_COPIER_HEADER_REJECT) {
        return MIPSNES2_CARTRIDGE_IMAGE_COPIER_HEADER_FORBIDDEN;
    }
    if (!has_copier_header && policy == MIPSNES2_COPIER_HEADER_REQUIRE) {
        return MIPSNES2_CARTRIDGE_IMAGE_COPIER_HEADER_REQUIRED;
    }
    (void)memset(view, 0, sizeof(*view));
    view->source_size = source_size;
    view->source_offset = has_copier_header
                              ? (size_t)MIPSNES2_COPIER_HEADER_SIZE
                              : 0U;
    view->data = source + view->source_offset;
    view->size = source_size - view->source_offset;
    view->had_copier_header = has_copier_header;
    return MIPSNES2_CARTRIDGE_IMAGE_OK;
}

mipsnes2_cartridge_image_result mipsnes2_cartridge_header_parse(
    const mipsnes2_cartridge_image_view *view,
    mipsnes2_cartridge_mapper candidate_mapper,
    mipsnes2_cartridge_header *header)
{
    const uint8_t *data;
    size_t offset;

    if (view == NULL || header == NULL || view->data == NULL) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    if (candidate_mapper == MIPSNES2_CARTRIDGE_LOROM) {
        offset = MIPSNES2_LOROM_HEADER_OFFSET;
    } else if (candidate_mapper == MIPSNES2_CARTRIDGE_HIROM) {
        offset = MIPSNES2_HIROM_HEADER_OFFSET;
    } else {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    if (view->size < offset ||
        view->size - offset < MIPSNES2_INTERNAL_HEADER_BLOCK_SIZE) {
        return MIPSNES2_CARTRIDGE_IMAGE_HEADER_OUT_OF_RANGE;
    }

    data = view->data + offset;
    (void)memset(header, 0, sizeof(*header));
    (void)memcpy(header->title, data, sizeof(header->title));
    header->map_mode = data[0x15];
    header->chipset = data[0x16];
    header->rom_size_exponent = data[0x17];
    header->sram_size_exponent = data[0x18];
    header->destination_code = data[0x19];
    header->developer_id = data[0x1a];
    header->version = data[0x1b];
    header->checksum_complement = read16(data + 0x1c);
    header->checksum = read16(data + 0x1e);
    header->reset_vector = read16(data + 0x3c);
    header->map = classify_map(header->map_mode);
    header->fastrom = (header->map_mode & UINT8_C(0x10)) != 0U;
    header->checksum_pair_valid =
        (uint16_t)(header->checksum ^ header->checksum_complement) ==
        UINT16_C(0xffff);
    header->declared_rom_size_representable = decode_size(
        header->rom_size_exponent, false, &header->declared_rom_size);
    header->declared_sram_size_representable = decode_size(
        header->sram_size_exponent, true, &header->declared_sram_size);
    return MIPSNES2_CARTRIDGE_IMAGE_OK;
}
