/* MIPSNES2 LoROM/HiROM cartridge mapping tests. See LICENSE for GPL-3.0 terms. */
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
    TEST_LOROM_SRAM_SIZE = 32 * 1024,
    TEST_HIROM_SRAM_SIZE = 8 * 1024
};

static uint8_t rom[TEST_ROM_SIZE];
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
    CHECK(mipsnes2_cartridge_make_interface(NULL).read8 == NULL);
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

int main(void)
{
    CHECK(test_configuration_validation() == 0);
    CHECK(test_lorom_mapping() == 0);
    CHECK(test_hirom_mapping() == 0);
    CHECK(test_snes_bus_cpu_integration() == 0);
    (void)puts("PASS: base LoROM/HiROM cartridge mapping foundation");
    return 0;
}
