/* MIPSNES2 LoROM/HiROM/ExHiROM mapping tests. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/snes_bus.h"
#include "core/cartridge/cartridge.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/cpu_timing.h"
#include "core/scheduler/scheduler.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

enum {
    TEST_ROM_SIZE = 256 * 1024,
    TEST_EXHIROM_SIZE = 6 * 1024 * 1024,
    TEST_LOROM_SRAM_SIZE = 32 * 1024,
    TEST_HIROM_SRAM_SIZE = 8 * 1024
};

static uint8_t rom[TEST_ROM_SIZE];
static uint8_t exhirom_rom[TEST_EXHIROM_SIZE];
static uint8_t sram[TEST_LOROM_SRAM_SIZE];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];

static mipsnes2_cartridge_config base_config(
    mipsnes2_cartridge_mapper mapper)
{
    mipsnes2_cartridge_config config;

    config.rom = rom;
    config.rom_size = sizeof(rom);
    config.sram = sram;
    config.sram_size = mapper == MIPSNES2_CARTRIDGE_LOROM
                           ? TEST_LOROM_SRAM_SIZE
                           : TEST_HIROM_SRAM_SIZE;
    config.mapper = mapper;
    return config;
}

static int test_configuration_validation(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    mipsnes2_cartridge_sram_view view;

    CHECK(mipsnes2_cartridge_init(NULL, &config) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cartridge_init(&cartridge, NULL) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    config.rom = NULL;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);
    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.sram = NULL;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_INVALID_ARGUMENT);

    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.mapper = (mipsnes2_cartridge_mapper)99;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_MAPPER);
    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.rom_size = MIPSNES2_LOROM_BANK_SIZE / 2U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config = base_config(MIPSNES2_CARTRIDGE_HIROM);
    config.rom_size = MIPSNES2_HIROM_BANK_SIZE / 2U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config.rom = exhirom_rom;
    config.rom_size = MIPSNES2_BASE_ROM_MAX_SIZE;
    config.mapper = MIPSNES2_CARTRIDGE_EXHIROM;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config.rom_size = MIPSNES2_EXHIROM_MAX_SIZE + 1U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.rom_size = UINT32_C(0x9000);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config.rom_size = MIPSNES2_BASE_ROM_MAX_SIZE + 1U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);

    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.sram_size = 3U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE);
    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.sram_size = MIPSNES2_LOROM_SRAM_MAX_SIZE + 1U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE);
    config = base_config(MIPSNES2_CARTRIDGE_HIROM);
    config.sram_size = MIPSNES2_HIROM_SRAM_MAX_SIZE + 1U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE);

    config = base_config(MIPSNES2_CARTRIDGE_HIROM);
    config.sram = NULL;
    config.sram_size = 0U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom == rom && cartridge.sram == NULL &&
          cartridge.mapper == MIPSNES2_CARTRIDGE_HIROM);
    CHECK(cartridge.rom_first_size == sizeof(rom));
    CHECK(cartridge.rom_mirror_span == sizeof(rom));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &view));
    CHECK(view.data == NULL && view.size == 0U);
    CHECK(view.revision == 0U && !view.dirty);
    CHECK(mipsnes2_cartridge_sram_acknowledge(&cartridge, 0U));
    CHECK(mipsnes2_cartridge_make_interface(NULL).read8 == NULL);
    return 0;
}

static int test_irregular_lorom_mapping(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    uint8_t value = 0U;

    (void)memset(rom, 0, sizeof(rom));
    config.rom_size = 96U * 1024U;
    config.sram = NULL;
    config.sram_size = 0U;
    rom[0x00000] = UINT8_C(0xa0);
    rom[0x08000] = UINT8_C(0xa1);
    rom[0x10000] = UINT8_C(0xa2);
    rom[0x17fff] = UINT8_C(0xa3);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom_first_size == (size_t)(64U * 1024U));
    CHECK(cartridge.rom_mirror_span == (size_t)(128U * 1024U));

    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x008000), &value));
    CHECK(value == UINT8_C(0xa0));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x018000), &value));
    CHECK(value == UINT8_C(0xa1));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x028000), &value));
    CHECK(value == UINT8_C(0xa2));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x038000), &value));
    CHECK(value == UINT8_C(0xa2));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x03ffff), &value));
    CHECK(value == UINT8_C(0xa3));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x048000), &value));
    CHECK(value == UINT8_C(0xa0));
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x038000),
                                    UINT8_C(0xff)));
    CHECK(rom[0x10000] == UINT8_C(0xa2));
    return 0;
}

static int test_irregular_hirom_mapping(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config = base_config(MIPSNES2_CARTRIDGE_HIROM);
    uint8_t value = 0U;

    (void)memset(rom, 0, sizeof(rom));
    config.rom_size = 192U * 1024U;
    config.sram = NULL;
    config.sram_size = 0U;
    rom[0x00000] = UINT8_C(0xb0);
    rom[0x10000] = UINT8_C(0xb1);
    rom[0x20000] = UINT8_C(0xb2);
    rom[0x2ffff] = UINT8_C(0xb3);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom_first_size == (size_t)(128U * 1024U));
    CHECK(cartridge.rom_mirror_span == (size_t)(256U * 1024U));

    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x400000), &value));
    CHECK(value == UINT8_C(0xb0));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x410000), &value));
    CHECK(value == UINT8_C(0xb1));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x420000), &value));
    CHECK(value == UINT8_C(0xb2));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x430000), &value));
    CHECK(value == UINT8_C(0xb2));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x43ffff), &value));
    CHECK(value == UINT8_C(0xb3));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x440000), &value));
    CHECK(value == UINT8_C(0xb0));
    return 0;
}

static int test_irregular_layout_rejection(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    uint8_t value = 0U;

    config.rom_size = 224U * 1024U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config.rom_size = 80U * 1024U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config = base_config(MIPSNES2_CARTRIDGE_HIROM);
    config.rom_size = 96U * 1024U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE);
    config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    config.rom_size = 160U * 1024U;
    (void)memset(rom, 0, sizeof(rom));
    rom[0] = UINT8_C(0xc2);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom_first_size == (size_t)(128U * 1024U));
    CHECK(cartridge.rom_mirror_span == (size_t)(256U * 1024U));
    rom[0x20000] = UINT8_C(0xc0);
    rom[0x27fff] = UINT8_C(0xc1);
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x048000), &value));
    CHECK(value == UINT8_C(0xc0));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x078000), &value));
    CHECK(value == UINT8_C(0xc0));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x07ffff), &value));
    CHECK(value == UINT8_C(0xc1));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x088000), &value));
    CHECK(value == UINT8_C(0xc2));
    return 0;
}

static int test_exhirom_mapping(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config;
    uint8_t value = 0U;

    (void)memset(exhirom_rom, 0, sizeof(exhirom_rom));
    config.rom = exhirom_rom;
    config.rom_size = sizeof(exhirom_rom);
    config.sram = sram;
    config.sram_size = TEST_HIROM_SRAM_SIZE;
    config.mapper = MIPSNES2_CARTRIDGE_EXHIROM;
    exhirom_rom[0x000000] = UINT8_C(0xd0);
    exhirom_rom[0x008000] = UINT8_C(0xd1);
    exhirom_rom[0x3fffff] = UINT8_C(0xd2);
    exhirom_rom[0x400000] = UINT8_C(0xd3);
    exhirom_rom[0x408000] = UINT8_C(0xd4);
    exhirom_rom[0x5fffff] = UINT8_C(0xd5);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(cartridge.rom_first_size == (size_t)(4U * 1024U * 1024U));
    CHECK(cartridge.rom_mirror_span == (size_t)(8U * 1024U * 1024U));

    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xc00000), &value));
    CHECK(value == UINT8_C(0xd0));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x808000), &value));
    CHECK(value == UINT8_C(0xd1));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xffffff), &value));
    CHECK(value == UINT8_C(0xd2));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x400000), &value));
    CHECK(value == UINT8_C(0xd3));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x008000), &value));
    CHECK(value == UINT8_C(0xd4));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x5fffff), &value));
    CHECK(value == UINT8_C(0xd5));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x600000), &value));
    CHECK(value == UINT8_C(0xd3));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x208000), &value));
    CHECK(value == UINT8_C(0xd4));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x3fffff), &value));
    CHECK(value == UINT8_C(0xd5));

    value = UINT8_C(0xcc);
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x007fff), &value));
    CHECK(value == UINT8_C(0xcc));
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x7e8000), &value));
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0xc00000),
                                    UINT8_C(0xff)));
    CHECK(exhirom_rom[0] == UINT8_C(0xd0));

    (void)memset(sram, 0, sizeof(sram));
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x206000),
                                    UINT8_C(0xd6)));
    CHECK(sram[0] == UINT8_C(0xd6));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xa06000), &value));
    CHECK(value == UINT8_C(0xd6));
    return 0;
}

static int test_lorom_mapping(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    uint8_t value = UINT8_C(0xcc);

    (void)memset(rom, 0, sizeof(rom));
    (void)memset(sram, 0, sizeof(sram));
    rom[0x00000] = UINT8_C(0x10);
    rom[0x07fff] = UINT8_C(0x11);
    rom[0x08000] = UINT8_C(0x12);
    rom[0x3ffff] = UINT8_C(0x13);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);

    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x008000), &value));
    CHECK(value == UINT8_C(0x10));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x00ffff), &value));
    CHECK(value == UINT8_C(0x11));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x018000), &value));
    CHECK(value == UINT8_C(0x12));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x6fffff), &value));
    CHECK(value == UINT8_C(0x13));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x408000), &value));
    CHECK(value == UINT8_C(0x10));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x808000), &value));
    CHECK(value == UINT8_C(0x10));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x01008000), &value));
    CHECK(value == UINT8_C(0x10));

    value = UINT8_C(0xcc);
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x007fff), &value));
    CHECK(value == UINT8_C(0xcc));
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x7e8000), &value));

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x700001),
                                    UINT8_C(0x31)));
    CHECK(sram[1] == UINT8_C(0x31));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xf00001), &value));
    CHECK(value == UINT8_C(0x31));
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x717fff),
                                    UINT8_C(0x32)));
    CHECK(sram[0x7fff] == UINT8_C(0x32));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x707fff), &value));
    CHECK(value == UINT8_C(0x32));

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x008000),
                                    UINT8_C(0x99)));
    CHECK(rom[0] == UINT8_C(0x10));
    CHECK(!mipsnes2_cartridge_write(&cartridge, UINT32_C(0x007fff),
                                     UINT8_C(0x99)));
    return 0;
}

static int test_hirom_mapping(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config = base_config(MIPSNES2_CARTRIDGE_HIROM);
    uint8_t value = UINT8_C(0xcc);

    (void)memset(rom, 0, sizeof(rom));
    (void)memset(sram, 0, sizeof(sram));
    rom[0x00000] = UINT8_C(0x20);
    rom[0x08000] = UINT8_C(0x21);
    rom[0x1ffff] = UINT8_C(0x22);
    rom[0x3ffff] = UINT8_C(0x23);
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);

    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x400000), &value));
    CHECK(value == UINT8_C(0x20));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x008000), &value));
    CHECK(value == UINT8_C(0x21));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x01ffff), &value));
    CHECK(value == UINT8_C(0x22));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x43ffff), &value));
    CHECK(value == UINT8_C(0x23));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xc00000), &value));
    CHECK(value == UINT8_C(0x20));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x808000), &value));
    CHECK(value == UINT8_C(0x21));

    value = UINT8_C(0xcc);
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x1f6000), &value));
    CHECK(value == UINT8_C(0xcc));
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x205fff), &value));
    CHECK(!mipsnes2_cartridge_read(&cartridge, UINT32_C(0x7e8000), &value));

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x206000),
                                    UINT8_C(0x41)));
    CHECK(sram[0] == UINT8_C(0x41));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0xa06000), &value));
    CHECK(value == UINT8_C(0x41));
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x217fff),
                                    UINT8_C(0x42)));
    CHECK(sram[0x1fff] == UINT8_C(0x42));
    CHECK(mipsnes2_cartridge_read(&cartridge, UINT32_C(0x207fff), &value));
    CHECK(value == UINT8_C(0x42));

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x400000),
                                    UINT8_C(0x99)));
    CHECK(rom[0] == UINT8_C(0x20));
    return 0;
}

static int test_sram_persistence_handoff(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config config =
        base_config(MIPSNES2_CARTRIDGE_LOROM);
    mipsnes2_cartridge_sram_view first;
    mipsnes2_cartridge_sram_view current;

    (void)memset(rom, 0, sizeof(rom));
    (void)memset(sram, 0, sizeof(sram));
    CHECK(mipsnes2_cartridge_init(&cartridge, &config) ==
          MIPSNES2_CARTRIDGE_OK);
    CHECK(!mipsnes2_cartridge_sram_view_get(NULL, &current));
    CHECK(!mipsnes2_cartridge_sram_view_get(&cartridge, NULL));
    CHECK(!mipsnes2_cartridge_sram_acknowledge(NULL, 0U));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.data == sram && current.size == TEST_LOROM_SRAM_SIZE);
    CHECK(current.revision == 0U && !current.dirty);
    CHECK(!mipsnes2_cartridge_sram_acknowledge(&cartridge, 1U));

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x008000),
                                    UINT8_C(0xaa)));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == 0U && !current.dirty);
    CHECK(!mipsnes2_cartridge_write(&cartridge, UINT32_C(0x007fff),
                                     UINT8_C(0xaa)));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == 0U && !current.dirty);

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x700003),
                                    UINT8_C(0x51)));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &first));
    CHECK(first.data == sram && first.data[3] == UINT8_C(0x51));
    CHECK(first.revision == UINT64_C(1) && first.dirty);
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0xf00003),
                                    UINT8_C(0x51)));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == first.revision && current.dirty);

    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x700003),
                                    UINT8_C(0x52)));
    CHECK(!mipsnes2_cartridge_sram_acknowledge(
              &cartridge, first.revision));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == UINT64_C(2) && current.dirty);
    CHECK(mipsnes2_cartridge_sram_acknowledge(
              &cartridge, current.revision));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == UINT64_C(2) && !current.dirty);
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x700003),
                                    UINT8_C(0x52)));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == UINT64_C(2) && !current.dirty);

    cartridge.sram_revision = UINT64_MAX;
    CHECK(mipsnes2_cartridge_write(&cartridge, UINT32_C(0x700003),
                                    UINT8_C(0x53)));
    CHECK(mipsnes2_cartridge_sram_view_get(&cartridge, &current));
    CHECK(current.revision == UINT64_C(1) && current.dirty);
    return 0;
}

static int test_snes_bus_cpu_integration(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config cartridge_config;
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config bus_config;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_report report;

    (void)memset(rom, 0, sizeof(rom));
    (void)memset(wram, 0, sizeof(wram));
    rom[0] = UINT8_C(0xea);
    rom[0x7ffc] = UINT8_C(0x00);
    rom[0x7ffd] = UINT8_C(0x80);
    cartridge_config = base_config(MIPSNES2_CARTRIDGE_LOROM);
    cartridge_config.rom_size = MIPSNES2_LOROM_BANK_SIZE;
    cartridge_config.sram = NULL;
    cartridge_config.sram_size = 0U;
    CHECK(mipsnes2_cartridge_init(&cartridge, &cartridge_config) ==
          MIPSNES2_CARTRIDGE_OK);

    (void)memset(&bus_config, 0, sizeof(bus_config));
    bus_config.wram = wram;
    bus_config.wram_size = sizeof(wram);
    bus_config.initial_open_value = UINT8_C(0xa5);
    bus_config.cartridge = mipsnes2_cartridge_make_interface(&cartridge);
    CHECK(mipsnes2_snes_bus_init(&bus, &bus_config) == MIPSNES2_SNES_BUS_OK);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x007000)) ==
          UINT8_C(0xa5));

    mipsnes2_cpu_init(&cpu, mipsnes2_snes_bus_make_interface(&bus));
    CHECK(mipsnes2_cpu_reset(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x8000));
    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 4U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(report.master_clocks == UINT64_C(14));
    return 0;
}

static int test_exhirom_snes_bus_cpu_integration(void)
{
    mipsnes2_cartridge cartridge;
    mipsnes2_cartridge_config cartridge_config;
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config bus_config;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_report report;

    (void)memset(exhirom_rom, 0, sizeof(exhirom_rom));
    (void)memset(wram, 0, sizeof(wram));
    exhirom_rom[0x408000] = UINT8_C(0xea);
    exhirom_rom[0x40fffc] = UINT8_C(0x00);
    exhirom_rom[0x40fffd] = UINT8_C(0x80);
    cartridge_config.rom = exhirom_rom;
    cartridge_config.rom_size = sizeof(exhirom_rom);
    cartridge_config.sram = NULL;
    cartridge_config.sram_size = 0U;
    cartridge_config.mapper = MIPSNES2_CARTRIDGE_EXHIROM;
    CHECK(mipsnes2_cartridge_init(&cartridge, &cartridge_config) ==
          MIPSNES2_CARTRIDGE_OK);

    (void)memset(&bus_config, 0, sizeof(bus_config));
    bus_config.wram = wram;
    bus_config.wram_size = sizeof(wram);
    bus_config.initial_open_value = UINT8_C(0xa5);
    bus_config.cartridge = mipsnes2_cartridge_make_interface(&cartridge);
    CHECK(mipsnes2_snes_bus_init(&bus, &bus_config) == MIPSNES2_SNES_BUS_OK);

    mipsnes2_cpu_init(&cpu, mipsnes2_snes_bus_make_interface(&bus));
    CHECK(mipsnes2_cpu_reset(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x8000));
    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 4U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(report.master_clocks == UINT64_C(14));
    return 0;
}

int main(void)
{
    CHECK(test_configuration_validation() == 0);
    CHECK(test_lorom_mapping() == 0);
    CHECK(test_hirom_mapping() == 0);
    CHECK(test_sram_persistence_handoff() == 0);
    CHECK(test_irregular_lorom_mapping() == 0);
    CHECK(test_irregular_hirom_mapping() == 0);
    CHECK(test_irregular_layout_rejection() == 0);
    CHECK(test_exhirom_mapping() == 0);
    CHECK(test_snes_bus_cpu_integration() == 0);
    CHECK(test_exhirom_snes_bus_cpu_integration() == 0);
    (void)puts("PASS: base LoROM/HiROM and ExHiROM mapping");
    return 0;
}
