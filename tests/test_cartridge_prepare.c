/* MIPSNES2 cartridge preparation tests. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/cartridge/prepare.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

enum {
    ROM_SIZE = 64 * 1024,
    PREFIXED_SIZE = ROM_SIZE + MIPSNES2_COPIER_HEADER_SIZE,
    SRAM_SIZE = 8 * 1024,
    SRAM_CAPACITY = 16 * 1024,
    EXHIROM_SIZE = 6 * 1024 * 1024
};

static uint8_t rom[ROM_SIZE];
static uint8_t prefixed_rom[PREFIXED_SIZE];
static uint8_t exhirom[EXHIROM_SIZE];
static uint8_t sram[SRAM_CAPACITY];

static void write16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void populate_header(uint8_t *payload,
                            size_t offset,
                            uint8_t map_mode,
                            uint8_t rom_exponent,
                            uint8_t sram_exponent)
{
    uint8_t *header = payload + offset;

    (void)memset(header - MIPSNES2_REGISTRATION_PREFIX_SIZE, 0,
                 MIPSNES2_REGISTRATION_PREFIX_SIZE +
                     MIPSNES2_INTERNAL_HEADER_BLOCK_SIZE);
    (void)memcpy(header, "MIPSNES2 PLAN TEST  ",
                 MIPSNES2_INTERNAL_TITLE_SIZE);
    header[0x15] = map_mode;
    header[0x16] = UINT8_C(0x02);
    header[0x17] = rom_exponent;
    header[0x18] = sram_exponent;
    write16(header + 0x3c, UINT16_C(0x8000));
}

static int finalize_header(uint8_t *source,
                           size_t source_size,
                           mipsnes2_copier_header_policy policy,
                           mipsnes2_cartridge_mapper mapper)
{
    mipsnes2_cartridge_image_view view;
    uint16_t checksum;
    size_t mirrored_size;
    size_t offset;
    uint8_t *header;

    CHECK(mipsnes2_cartridge_image_view_init(
              &view, source, source_size, policy) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
              &view, mapper, &checksum, &mirrored_size) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    if (mapper == MIPSNES2_CARTRIDGE_LOROM) {
        offset = MIPSNES2_LOROM_HEADER_OFFSET;
    } else if (mapper == MIPSNES2_CARTRIDGE_HIROM) {
        offset = MIPSNES2_HIROM_HEADER_OFFSET;
    } else {
        offset = MIPSNES2_EXHIROM_HEADER_OFFSET;
    }
    header = (uint8_t *)view.data + offset;
    write16(header + 0x1c, (uint16_t)(checksum ^ UINT16_C(0xffff)));
    write16(header + 0x1e, checksum);
    return 0;
}

static int make_lorom(uint8_t *source,
                      size_t source_size,
                      size_t payload_offset,
                      uint8_t sram_exponent)
{
    uint8_t *payload = source + payload_offset;

    (void)memset(source, 0, source_size);
    payload[0] = UINT8_C(0x42);
    populate_header(payload, MIPSNES2_LOROM_HEADER_OFFSET,
                    UINT8_C(0x20), UINT8_C(6), sram_exponent);
    return finalize_header(source, source_size,
                           payload_offset == 0U
                               ? MIPSNES2_COPIER_HEADER_REJECT
                               : MIPSNES2_COPIER_HEADER_REQUIRE,
                           MIPSNES2_CARTRIDGE_LOROM);
}

static int test_plan_validation_and_transactionality(void)
{
    mipsnes2_cartridge_plan plan;
    mipsnes2_cartridge_plan before;
    uint8_t malformed[MIPSNES2_LOROM_BANK_SIZE + 1U];

    (void)memset(&plan, UINT8_C(0xa5), sizeof(plan));
    before = plan;
    CHECK(mipsnes2_cartridge_plan_create(
              NULL, rom, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, NULL, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT);
    CHECK(memcmp(&plan, &before, sizeof(plan)) == 0);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, rom, sizeof(rom),
              (mipsnes2_copier_header_policy)99) ==
          MIPSNES2_CARTRIDGE_IMAGE_INVALID_POLICY);
    CHECK(memcmp(&plan, &before, sizeof(plan)) == 0);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, malformed, sizeof(malformed),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE);
    CHECK(memcmp(&plan, &before, sizeof(plan)) == 0);

    (void)memset(rom, 0, sizeof(rom));
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, rom, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER);
    CHECK(memcmp(&plan, &before, sizeof(plan)) == 0);
    return 0;
}

static int test_no_sram_and_prefixed_construction(void)
{
    mipsnes2_cartridge_plan plan;
    mipsnes2_cartridge cartridge;
    uint8_t value = 0U;

    CHECK(make_lorom(rom, sizeof(rom), 0U, 0U) == 0);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, rom, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(plan.image.data == rom && plan.image.source_offset == 0U);
    CHECK(plan.selection.selected_mapper == MIPSNES2_CARTRIDGE_LOROM);
    CHECK(plan.required_sram_size == 0U);
    CHECK(mipsnes2_cartridge_construct(&cartridge, &plan, NULL, 0U) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom == rom && cartridge.sram == NULL);
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x008000), &value));
    CHECK(value == UINT8_C(0x42));

    CHECK(make_lorom(prefixed_rom, sizeof(prefixed_rom),
                     MIPSNES2_COPIER_HEADER_SIZE, 0U) == 0);
    (void)memset(prefixed_rom, UINT8_C(0xa5),
                 MIPSNES2_COPIER_HEADER_SIZE);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, prefixed_rom, sizeof(prefixed_rom),
              MIPSNES2_COPIER_HEADER_REQUIRE) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(plan.image.had_copier_header);
    CHECK(plan.image.source_offset == MIPSNES2_COPIER_HEADER_SIZE);
    CHECK(plan.image.data == prefixed_rom + MIPSNES2_COPIER_HEADER_SIZE);
    CHECK(mipsnes2_cartridge_construct(&cartridge, &plan, NULL, 0U) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom == prefixed_rom + MIPSNES2_COPIER_HEADER_SIZE);
    return 0;
}

static int test_sram_capacity_and_failed_output(void)
{
    mipsnes2_cartridge_plan plan;
    mipsnes2_cartridge_plan valid_plan;
    mipsnes2_cartridge invalid_plan_cartridge;
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge before;
    uint8_t value = 0U;

    CHECK(make_lorom(rom, sizeof(rom), 0U, UINT8_C(3)) == 0);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, rom, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(plan.required_sram_size == SRAM_SIZE);
    valid_plan = plan;

    (void)memset(&cartridge, UINT8_C(0x5a), sizeof(cartridge));
    before = cartridge;
    CHECK(mipsnes2_cartridge_construct(&cartridge, &plan, NULL, 0U) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE);
    CHECK(memcmp(&cartridge, &before, sizeof(cartridge)) == 0);
    CHECK(mipsnes2_cartridge_construct(
              &cartridge, &plan, sram, SRAM_SIZE - 1U) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE);
    CHECK(memcmp(&cartridge, &before, sizeof(cartridge)) == 0);
    CHECK(mipsnes2_cartridge_construct(
              &cartridge, &plan, NULL, SRAM_SIZE) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    CHECK(memcmp(&cartridge, &before, sizeof(cartridge)) == 0);

    (void)memset(sram, 0, sizeof(sram));
    CHECK(mipsnes2_cartridge_construct(
              &cartridge, &plan, sram, sizeof(sram)) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.sram == sram && cartridge.sram_size == SRAM_SIZE);
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x700000),
                                    UINT8_C(0x91)));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xf00000), &value));
    CHECK(value == UINT8_C(0x91));

    CHECK(mipsnes2_cartridge_construct(NULL, &plan, sram, sizeof(sram)) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_construct(
              &invalid_plan_cartridge, NULL, sram, sizeof(sram)) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    plan.required_sram_size++;
    CHECK(mipsnes2_cartridge_construct(
              &invalid_plan_cartridge, &plan, sram, sizeof(sram)) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    plan = valid_plan;
    plan.image.size++;
    CHECK(mipsnes2_cartridge_construct(
              &invalid_plan_cartridge, &plan, sram, sizeof(sram)) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    return 0;
}

static int test_exhirom_construction(void)
{
    mipsnes2_cartridge_plan plan;
    mipsnes2_cartridge cartridge;
    uint8_t value = 0U;

    (void)memset(exhirom, 0, sizeof(exhirom));
    exhirom[0] = UINT8_C(0x81);
    exhirom[4U * 1024U * 1024U] = UINT8_C(0x82);
    populate_header(exhirom, MIPSNES2_EXHIROM_HEADER_OFFSET,
                    UINT8_C(0x25), UINT8_C(13), 0U);
    CHECK(finalize_header(exhirom, sizeof(exhirom),
                          MIPSNES2_COPIER_HEADER_REJECT,
                          MIPSNES2_CARTRIDGE_EXHIROM) == 0);
    CHECK(mipsnes2_cartridge_plan_create(
              &plan, exhirom, sizeof(exhirom),
              MIPSNES2_COPIER_HEADER_REJECT) ==
          MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(plan.selection.selected_mapper == MIPSNES2_CARTRIDGE_EXHIROM);
    CHECK(plan.required_sram_size == 0U);
    CHECK(mipsnes2_cartridge_construct(&cartridge, &plan, NULL, 0U) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xc00000), &value));
    CHECK(value == UINT8_C(0x81));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x400000), &value));
    CHECK(value == UINT8_C(0x82));
    return 0;
}

int main(void)
{
    CHECK(test_plan_validation_and_transactionality() == 0);
    CHECK(test_no_sram_and_prefixed_construction() == 0);
    CHECK(test_sram_capacity_and_failed_output() == 0);
    CHECK(test_exhirom_construction() == 0);
    (void)puts("PASS: platform-neutral cartridge preparation and construction");
    return 0;
}
