/* MIPSNES2 cartridge-image metadata tests. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/cartridge/image.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

enum {
    CLEAN_SIZE = 64 * 1024,
    HEADERED_SIZE = CLEAN_SIZE + MIPSNES2_COPIER_HEADER_SIZE,
    IRREGULAR_SIZE = 96 * 1024,
    PADDED_IRREGULAR_SIZE = 224 * 1024
};

static uint8_t clean_image[CLEAN_SIZE];
static uint8_t headered_image[HEADERED_SIZE];
static uint8_t irregular_image[IRREGULAR_SIZE];
static uint8_t padded_irregular_image[PADDED_IRREGULAR_SIZE];

static void write16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void populate_header(uint8_t *data,
                            size_t offset,
                            uint8_t map_mode,
                            uint8_t rom_size,
                            uint8_t sram_size)
{
    static const uint8_t title[MIPSNES2_INTERNAL_TITLE_SIZE] =
        "MIPSNES2 HEADER TEST ";
    uint8_t *header = data + offset;

    (void)memset(header, 0, MIPSNES2_INTERNAL_HEADER_BLOCK_SIZE);
    (void)memcpy(header, title, sizeof(title));
    header[0x15] = map_mode;
    header[0x16] = UINT8_C(0x02);
    header[0x17] = rom_size;
    header[0x18] = sram_size;
    header[0x19] = UINT8_C(0x01);
    header[0x1a] = UINT8_C(0x33);
    header[0x1b] = UINT8_C(0x07);
    write16(header + 0x1c, UINT16_C(0x4567));
    write16(header + 0x1e, UINT16_C(0xba98));
    write16(header + 0x3c, UINT16_C(0x8123));
}

static size_t candidate_offset(mipsnes2_cartridge_mapper mapper)
{
    return mapper == MIPSNES2_CARTRIDGE_LOROM
               ? (size_t)MIPSNES2_LOROM_HEADER_OFFSET
               : (size_t)MIPSNES2_HIROM_HEADER_OFFSET;
}

static int finalize_candidate(mipsnes2_cartridge_image_view *view,
                              mipsnes2_cartridge_mapper mapper)
{
    uint16_t checksum;
    size_t mirrored_size;
    uint8_t *header;

    CHECK(mipsnes2_cartridge_checksum_calculate(
              view, mapper, &checksum, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    header = (uint8_t *)view->data + candidate_offset(mapper);
    write16(header + 0x1c, (uint16_t)(checksum ^ UINT16_C(0xffff)));
    write16(header + 0x1e, checksum);
    return 0;
}

static int test_image_view_validation(void)
{
    mipsnes2_cartridge_image_view view;

    CHECK(mipsnes2_cartridge_image_view_init(
              NULL, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, NULL, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              (mipsnes2_copier_header_policy)99) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_POLICY);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, 0U, MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, headered_image, MIPSNES2_COPIER_HEADER_SIZE,
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, MIPSNES2_LOROM_BANK_SIZE + 1U,
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE);

    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_REQUIRE) ==
          MIPSNES2_CARTRIDGE_IMAGE_COPIER_HEADER_REQUIRED);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, headered_image, sizeof(headered_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_COPIER_HEADER_FORBIDDEN);

    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(view.data == clean_image && view.size == sizeof(clean_image));
    CHECK(view.source_size == sizeof(clean_image) && view.source_offset == 0U);
    CHECK(!view.had_copier_header);

    CHECK(mipsnes2_cartridge_image_view_init(
              &view, headered_image, sizeof(headered_image),
              MIPSNES2_COPIER_HEADER_REQUIRE) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(view.data == headered_image + MIPSNES2_COPIER_HEADER_SIZE);
    CHECK(view.size == CLEAN_SIZE);
    CHECK(view.source_size == sizeof(headered_image));
    CHECK(view.source_offset == MIPSNES2_COPIER_HEADER_SIZE);
    CHECK(view.had_copier_header);

    CHECK(mipsnes2_cartridge_image_view_init(
              &view, irregular_image, sizeof(irregular_image),
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(view.size == sizeof(irregular_image));
    return 0;
}

static int test_lorom_header_parse(void)
{
    static const uint8_t expected_title[MIPSNES2_INTERNAL_TITLE_SIZE] =
        "MIPSNES2 HEADER TEST ";
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header header;
    uint8_t *raw;

    (void)memset(clean_image, 0, sizeof(clean_image));
    populate_header(clean_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x30), UINT8_C(8), UINT8_C(5));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_LOROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(memcmp(header.title, expected_title, sizeof(expected_title)) == 0);
    CHECK(header.map_mode == UINT8_C(0x30));
    CHECK(header.chipset == UINT8_C(0x02));
    CHECK(header.rom_size_exponent == UINT8_C(8));
    CHECK(header.sram_size_exponent == UINT8_C(5));
    CHECK(header.destination_code == UINT8_C(0x01));
    CHECK(header.developer_id == UINT8_C(0x33));
    CHECK(header.version == UINT8_C(0x07));
    CHECK(header.checksum_complement == UINT16_C(0x4567));
    CHECK(header.checksum == UINT16_C(0xba98));
    CHECK(header.reset_vector == UINT16_C(0x8123));
    CHECK(header.map == MIPSNES2_HEADER_MAP_BASE_LOROM);
    CHECK(header.fastrom && header.checksum_pair_valid);
    CHECK(header.declared_rom_size_representable);
    CHECK(header.declared_rom_size == (size_t)(256 * 1024));
    CHECK(header.declared_sram_size_representable);
    CHECK(header.declared_sram_size == (size_t)(32 * 1024));

    raw = clean_image + MIPSNES2_LOROM_HEADER_OFFSET;
    raw[0x1c] ^= UINT8_C(1);
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_LOROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(!header.checksum_pair_valid);
    return 0;
}

static int test_hirom_and_candidate_boundaries(void)
{
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header header;

    (void)memset(clean_image, 0, sizeof(clean_image));
    populate_header(clean_image, MIPSNES2_HIROM_HEADER_OFFSET,
                    UINT8_C(0x21), UINT8_C(10), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_HIROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(header.map == MIPSNES2_HEADER_MAP_BASE_HIROM);
    CHECK(!header.fastrom);
    CHECK(header.declared_rom_size == (size_t)(1024 * 1024));
    CHECK(header.declared_sram_size_representable);
    CHECK(header.declared_sram_size == 0U);

    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_LOROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(header.map == MIPSNES2_HEADER_MAP_OTHER);

    view.size = MIPSNES2_LOROM_BANK_SIZE;
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_HIROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_HEADER_OUT_OF_RANGE);
    CHECK(mipsnes2_cartridge_header_parse(
              &view, (mipsnes2_cartridge_mapper)99, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_header_parse(NULL,
              MIPSNES2_CARTRIDGE_LOROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_LOROM, NULL) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    view.data = NULL;
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_LOROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    return 0;
}

static int test_copier_offset_and_size_overflow(void)
{
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header header;
    size_t offset = MIPSNES2_COPIER_HEADER_SIZE +
                    MIPSNES2_LOROM_HEADER_OFFSET;

    (void)memset(headered_image, UINT8_C(0xcc), sizeof(headered_image));
    populate_header(headered_image, offset, UINT8_C(0x20),
                    UINT8_MAX, UINT8_MAX);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, headered_image, sizeof(headered_image),
              MIPSNES2_COPIER_HEADER_DETECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_header_parse(
              &view, MIPSNES2_CARTRIDGE_LOROM, &header) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(header.map == MIPSNES2_HEADER_MAP_BASE_LOROM);
    CHECK(!header.fastrom);
    CHECK(!header.declared_rom_size_representable);
    CHECK(header.declared_rom_size == 0U);
    CHECK(!header.declared_sram_size_representable);
    CHECK(header.declared_sram_size == 0U);
    return 0;
}

static int test_checksum_calculation(void)
{
    mipsnes2_cartridge_image_view view;
    uint16_t checksum;
    size_t mirrored_size;

    (void)memset(clean_image, 0, sizeof(clean_image));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) == MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(checksum == UINT16_C(0x01fe));
    CHECK(mirrored_size == CLEAN_SIZE);

    write16(clean_image + MIPSNES2_LOROM_HEADER_OFFSET + 0x1c,
            UINT16_C(0x1234));
    write16(clean_image + MIPSNES2_LOROM_HEADER_OFFSET + 0x1e,
            UINT16_C(0xabcd));
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) == MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(checksum == UINT16_C(0x01fe));

    (void)memset(headered_image, UINT8_C(0xa5),
                 MIPSNES2_COPIER_HEADER_SIZE);
    (void)memset(headered_image + MIPSNES2_COPIER_HEADER_SIZE, 0,
                 CLEAN_SIZE);
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, headered_image, sizeof(headered_image),
              MIPSNES2_COPIER_HEADER_REQUIRE) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) == MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(checksum == UINT16_C(0x01fe));
    CHECK(mirrored_size == CLEAN_SIZE);

    (void)memset(irregular_image, 0, sizeof(irregular_image));
    irregular_image[sizeof(irregular_image) - 1U] = 1U;
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, irregular_image, sizeof(irregular_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) == MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(checksum == UINT16_C(0x0200));
    CHECK(mirrored_size == (size_t)(128 * 1024));

    (void)memset(padded_irregular_image, 0,
                 sizeof(padded_irregular_image));
    padded_irregular_image[sizeof(padded_irregular_image) - 1U] = 1U;
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, padded_irregular_image,
              sizeof(padded_irregular_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) == MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(checksum == UINT16_C(0x01ff));
    CHECK(mirrored_size == (size_t)(256 * 1024));

    CHECK(mipsnes2_cartridge_checksum_calculate(
              NULL, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, (mipsnes2_cartridge_mapper)99,
              &checksum, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              NULL, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, NULL) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    view.size = MIPSNES2_LOROM_BANK_SIZE;
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_HIROM,
              &checksum, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_HEADER_OUT_OF_RANGE);
    view.data = NULL;
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    return 0;
}

static int test_base_mapper_selection(void)
{
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header_selection selection;
    uint8_t *header;

    (void)memset(clean_image, 0, sizeof(clean_image));
    populate_header(clean_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(6), UINT8_C(0));
    header = clean_image + MIPSNES2_LOROM_HEADER_OFFSET;
    (void)memset(header, UINT8_C(0xff), MIPSNES2_INTERNAL_TITLE_SIZE);
    header[0x16] = 0U;
    write16(header + 0x3c, UINT16_C(0x8000));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(selection.selected_mapper == MIPSNES2_CARTRIDGE_LOROM);
    CHECK(selection.lorom.header_available);
    CHECK(selection.lorom.map_mode_matches);
    CHECK(selection.lorom.base_chipset_supported);
    CHECK(selection.lorom.reset_vector_in_rom);
    CHECK(selection.lorom.declared_rom_size_matches);
    CHECK(selection.lorom.declared_sram_size_supported);
    CHECK(selection.lorom.checksum_layout_unambiguous);
    CHECK(selection.lorom.header.checksum_pair_valid);
    CHECK(selection.lorom.checksum_matches && selection.lorom.confident);
    CHECK(selection.lorom.mirrored_size == CLEAN_SIZE);
    CHECK(selection.hirom.header_available && !selection.hirom.confident);

    (void)memset(clean_image, 0, sizeof(clean_image));
    populate_header(clean_image, MIPSNES2_HIROM_HEADER_OFFSET,
                    UINT8_C(0x31), UINT8_C(6), UINT8_C(0));
    header = clean_image + MIPSNES2_HIROM_HEADER_OFFSET;
    header[0x16] = 1U;
    write16(header + 0x3c, UINT16_C(0xff00));
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_HIROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(selection.selected_mapper == MIPSNES2_CARTRIDGE_HIROM);
    CHECK(selection.hirom.confident && !selection.lorom.confident);
    return 0;
}

static int test_selection_prefix_irregular_and_ambiguity(void)
{
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header_selection selection;
    uint16_t checksum;
    size_t mirrored_size;

    (void)memset(headered_image, UINT8_C(0x5a),
                 MIPSNES2_COPIER_HEADER_SIZE);
    (void)memset(headered_image + MIPSNES2_COPIER_HEADER_SIZE, 0,
                 CLEAN_SIZE);
    populate_header(headered_image,
                    MIPSNES2_COPIER_HEADER_SIZE +
                        MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(6), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, headered_image, sizeof(headered_image),
              MIPSNES2_COPIER_HEADER_REQUIRE) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(selection.selected_mapper == MIPSNES2_CARTRIDGE_LOROM);

    (void)memset(irregular_image, 0, sizeof(irregular_image));
    populate_header(irregular_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(7), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, irregular_image, sizeof(irregular_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(selection.selected_mapper == MIPSNES2_CARTRIDGE_LOROM);
    CHECK(selection.lorom.mirrored_size == (size_t)(128 * 1024));
    CHECK(selection.lorom.checksum_layout_unambiguous);

    (void)memset(clean_image, 0, sizeof(clean_image));
    populate_header(clean_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(6), UINT8_C(0));
    populate_header(clean_image, MIPSNES2_HIROM_HEADER_OFFSET,
                    UINT8_C(0x21), UINT8_C(6), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &checksum, &mirrored_size) == MIPSNES2_CARTRIDGE_IMAGE_OK);
    write16(clean_image + MIPSNES2_LOROM_HEADER_OFFSET + 0x1c,
            (uint16_t)(checksum ^ UINT16_C(0xffff)));
    write16(clean_image + MIPSNES2_LOROM_HEADER_OFFSET + 0x1e, checksum);
    write16(clean_image + MIPSNES2_HIROM_HEADER_OFFSET + 0x1c,
            (uint16_t)(checksum ^ UINT16_C(0xffff)));
    write16(clean_image + MIPSNES2_HIROM_HEADER_OFFSET + 0x1e, checksum);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_AMBIGUOUS_HEADER);
    CHECK(selection.lorom.confident && selection.hirom.confident);
    return 0;
}

static int test_selection_rejection_evidence(void)
{
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header_selection selection;
    uint8_t *header;

    (void)memset(clean_image, 0, sizeof(clean_image));
    populate_header(clean_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(6), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, sizeof(clean_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    header = clean_image + MIPSNES2_LOROM_HEADER_OFFSET;

    clean_image[0] ^= 1U;
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(selection.lorom.header.checksum_pair_valid);
    CHECK(!selection.lorom.checksum_matches && !selection.lorom.confident);
    clean_image[0] ^= 1U;

    header[0x1c] ^= 1U;
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(selection.lorom.checksum_matches);
    CHECK(!selection.lorom.header.checksum_pair_valid);
    header[0x1c] ^= 1U;

    write16(header + 0x3c, UINT16_C(0x7fff));
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(!selection.lorom.reset_vector_in_rom);

    write16(header + 0x3c, UINT16_C(0x8000));
    header[0x15] = UINT8_C(0x21);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(!selection.lorom.map_mode_matches);

    header[0x15] = UINT8_C(0x20);
    header[0x17] = UINT8_C(7);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(!selection.lorom.declared_rom_size_matches);

    header[0x17] = UINT8_C(6);
    header[0x16] = UINT8_C(3);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(!selection.lorom.base_chipset_supported);

    header[0x16] = 0U;
    header[0x18] = UINT8_MAX;
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(!selection.lorom.declared_sram_size_supported);

    (void)memset(padded_irregular_image, 0,
                 sizeof(padded_irregular_image));
    populate_header(padded_irregular_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(8), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, padded_irregular_image,
              sizeof(padded_irregular_image),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(!selection.lorom.checksum_layout_unambiguous);
    CHECK(selection.lorom.checksum_matches);

    CHECK(mipsnes2_cartridge_select_base_mapper(NULL, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, NULL) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    view.data = NULL;
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);

    view.data = clean_image;
    view.size = MIPSNES2_LOROM_BANK_SIZE + 1U;
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, MIPSNES2_CARTRIDGE_LOROM,
              &selection.lorom.calculated_checksum,
              &selection.lorom.mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE);
    return 0;
}

static int test_single_lorom_candidate(void)
{
    mipsnes2_cartridge_image_view view;
    mipsnes2_cartridge_header_selection selection;

    (void)memset(clean_image, 0, MIPSNES2_LOROM_BANK_SIZE);
    populate_header(clean_image, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(5), UINT8_C(0));
    CHECK(mipsnes2_cartridge_image_view_init(
              &view, clean_image, MIPSNES2_LOROM_BANK_SIZE,
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(finalize_candidate(&view, MIPSNES2_CARTRIDGE_LOROM) == 0);
    CHECK(mipsnes2_cartridge_select_base_mapper(&view, &selection) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(selection.selected_mapper == MIPSNES2_CARTRIDGE_LOROM);
    CHECK(selection.lorom.confident);
    CHECK(!selection.hirom.header_available);
    return 0;
}

int main(void)
{
    CHECK(test_image_view_validation() == 0);
    CHECK(test_lorom_header_parse() == 0);
    CHECK(test_hirom_and_candidate_boundaries() == 0);
    CHECK(test_copier_offset_and_size_overflow() == 0);
    CHECK(test_checksum_calculation() == 0);
    CHECK(test_base_mapper_selection() == 0);
    CHECK(test_selection_prefix_irregular_and_ambiguity() == 0);
    CHECK(test_selection_rejection_evidence() == 0);
    CHECK(test_single_lorom_candidate() == 0);
    (void)puts("PASS: cartridge image checksum and base-mapper selection");
    return 0;
}
