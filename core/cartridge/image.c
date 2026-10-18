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

static bool header_offset_for_mapper(mipsnes2_cartridge_mapper mapper,
                                     size_t *offset)
{
    if (mapper == MIPSNES2_CARTRIDGE_LOROM) {
        *offset = MIPSNES2_LOROM_HEADER_OFFSET;
        return true;
    }
    if (mapper == MIPSNES2_CARTRIDGE_HIROM) {
        *offset = MIPSNES2_HIROM_HEADER_OFFSET;
        return true;
    }
    return false;
}

static size_t largest_power_of_two(size_t value)
{
    size_t power = 1U;

    while (power <= value / 2U) {
        power *= 2U;
    }
    return power;
}

static bool is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

static uint16_t checksum_range(const uint8_t *data,
                               size_t begin,
                               size_t end,
                               size_t checksum_fields)
{
    uint16_t sum = 0U;
    size_t index;

    for (index = begin; index < end; ++index) {
        uint8_t value = data[index];

        if (index == checksum_fields || index == checksum_fields + 1U) {
            value = UINT8_C(0xff);
        } else if (index == checksum_fields + 2U ||
                   index == checksum_fields + 3U) {
            value = 0U;
        }
        sum = (uint16_t)(sum + value);
    }
    return sum;
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
    if (!header_offset_for_mapper(candidate_mapper, &offset)) {
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

mipsnes2_cartridge_image_result mipsnes2_cartridge_checksum_calculate(
    const mipsnes2_cartridge_image_view *view,
    mipsnes2_cartridge_mapper candidate_mapper,
    uint16_t *checksum,
    size_t *mirrored_size)
{
    size_t header_offset;
    size_t first_size;
    size_t remainder_size;
    size_t remainder_span;
    size_t repeat_count;
    uint16_t first_sum;
    uint16_t remainder_sum;
    uint32_t combined;

    if (view == NULL || view->data == NULL || checksum == NULL ||
        mirrored_size == NULL || view->size == 0U) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    if (view->size % (size_t)MIPSNES2_LOROM_BANK_SIZE != 0U) {
        return MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE;
    }
    if (!header_offset_for_mapper(candidate_mapper, &header_offset)) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    if (view->size < header_offset ||
        view->size - header_offset < MIPSNES2_INTERNAL_HEADER_BLOCK_SIZE) {
        return MIPSNES2_CARTRIDGE_IMAGE_HEADER_OUT_OF_RANGE;
    }

    first_size = largest_power_of_two(view->size);
    first_sum = checksum_range(view->data, 0U, first_size,
                               header_offset + 0x1cU);
    remainder_size = view->size - first_size;
    if (remainder_size == 0U) {
        *checksum = first_sum;
        *mirrored_size = first_size;
        return MIPSNES2_CARTRIDGE_IMAGE_OK;
    }
    if (first_size > SIZE_MAX / 2U) {
        return MIPSNES2_CARTRIDGE_IMAGE_SIZE_OVERFLOW;
    }

    remainder_span = largest_power_of_two(remainder_size);
    if (remainder_span != remainder_size) {
        if (remainder_span > SIZE_MAX / 2U) {
            return MIPSNES2_CARTRIDGE_IMAGE_SIZE_OVERFLOW;
        }
        remainder_span *= 2U;
    }
    repeat_count = first_size / remainder_span;
    remainder_sum = checksum_range(view->data, first_size, view->size,
                                   header_offset + 0x1cU);
    combined = (uint32_t)first_sum +
               (uint32_t)remainder_sum *
                   (uint32_t)(repeat_count & UINT16_MAX);
    *checksum = (uint16_t)combined;
    *mirrored_size = first_size * 2U;
    return MIPSNES2_CARTRIDGE_IMAGE_OK;
}

static mipsnes2_cartridge_image_result evaluate_candidate(
    const mipsnes2_cartridge_image_view *view,
    mipsnes2_cartridge_mapper mapper,
    mipsnes2_cartridge_header_candidate *candidate)
{
    mipsnes2_cartridge_header_map expected_map;
    mipsnes2_cartridge_image_result result;
    size_t remainder_size;

    (void)memset(candidate, 0, sizeof(*candidate));
    result = mipsnes2_cartridge_header_parse(view, mapper,
                                             &candidate->header);
    if (result == MIPSNES2_CARTRIDGE_IMAGE_HEADER_OUT_OF_RANGE) {
        return MIPSNES2_CARTRIDGE_IMAGE_OK;
    }
    if (result != MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return result;
    }
    result = mipsnes2_cartridge_checksum_calculate(
        view, mapper, &candidate->calculated_checksum,
        &candidate->mirrored_size);
    if (result != MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return result;
    }

    expected_map = mapper == MIPSNES2_CARTRIDGE_LOROM
                       ? MIPSNES2_HEADER_MAP_BASE_LOROM
                       : MIPSNES2_HEADER_MAP_BASE_HIROM;
    candidate->header_available = true;
    candidate->map_mode_matches = candidate->header.map == expected_map;
    candidate->base_chipset_supported = candidate->header.chipset <= 2U;
    candidate->reset_vector_in_rom =
        candidate->header.reset_vector >= UINT16_C(0x8000);
    candidate->declared_rom_size_matches =
        candidate->header.declared_rom_size_representable &&
        candidate->header.declared_rom_size == candidate->mirrored_size;
    candidate->declared_sram_size_supported =
        candidate->header.declared_sram_size_representable &&
        candidate->header.declared_sram_size <=
            (mapper == MIPSNES2_CARTRIDGE_LOROM
                 ? (size_t)MIPSNES2_LOROM_SRAM_MAX_SIZE
                 : (size_t)MIPSNES2_HIROM_SRAM_MAX_SIZE);
    remainder_size = view->size - largest_power_of_two(view->size);
    candidate->checksum_layout_unambiguous =
        remainder_size == 0U || is_power_of_two(remainder_size);
    candidate->checksum_matches =
        candidate->header.checksum == candidate->calculated_checksum;
    candidate->confident =
        candidate->map_mode_matches && candidate->base_chipset_supported &&
        candidate->reset_vector_in_rom &&
        candidate->declared_rom_size_matches &&
        candidate->declared_sram_size_supported &&
        candidate->checksum_layout_unambiguous &&
        candidate->header.checksum_pair_valid && candidate->checksum_matches;
    return MIPSNES2_CARTRIDGE_IMAGE_OK;
}

mipsnes2_cartridge_image_result mipsnes2_cartridge_select_base_mapper(
    const mipsnes2_cartridge_image_view *view,
    mipsnes2_cartridge_header_selection *selection)
{
    mipsnes2_cartridge_image_result result;

    if (view == NULL || view->data == NULL || selection == NULL) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    (void)memset(selection, 0, sizeof(*selection));
    result = evaluate_candidate(view, MIPSNES2_CARTRIDGE_LOROM,
                                &selection->lorom);
    if (result != MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return result;
    }
    result = evaluate_candidate(view, MIPSNES2_CARTRIDGE_HIROM,
                                &selection->hirom);
    if (result != MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return result;
    }
    if (selection->lorom.confident && selection->hirom.confident) {
        return MIPSNES2_CARTRIDGE_IMAGE_AMBIGUOUS_HEADER;
    }
    if (!selection->lorom.confident && !selection->hirom.confident) {
        return MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER;
    }
    selection->selected_mapper = selection->lorom.confident
                                     ? MIPSNES2_CARTRIDGE_LOROM
                                     : MIPSNES2_CARTRIDGE_HIROM;
    return MIPSNES2_CARTRIDGE_IMAGE_OK;
}
