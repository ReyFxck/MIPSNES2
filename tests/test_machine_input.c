/* Integrated machine standard-joypad injection fixture. See LICENSE. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/machine/machine.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t rom[64U * 1024U];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[8U * 1024U];
static mipsnes2_machine machine;

static void put16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static int make_image(void)
{
    mipsnes2_cartridge_image_view view;
    uint8_t *header = rom + MIPSNES2_LOROM_HEADER_OFFSET;
    uint16_t checksum;
    size_t mirrored;

    (void)memset(rom, 0, sizeof(rom));
    rom[0] = UINT8_C(0x80); /* BRA -2 */
    rom[1] = UINT8_C(0xfe);
    (void)memcpy(header, "MIPSNES2 INPUT TEST ", MIPSNES2_INTERNAL_TITLE_SIZE);
    header[0x15] = UINT8_C(0x20);
    header[0x16] = UINT8_C(0x02);
    header[0x17] = UINT8_C(6);
    header[0x18] = UINT8_C(3);
    put16(header + 0x3c, UINT16_C(0x8000));
    CHECK(mipsnes2_cartridge_image_view_init(
        &view, rom, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
        MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
        &view, MIPSNES2_CARTRIDGE_LOROM, &checksum, &mirrored) ==
        MIPSNES2_CARTRIDGE_IMAGE_OK);
    put16(header + 0x1c, (uint16_t)(checksum ^ UINT16_C(0xffff)));
    put16(header + 0x1e, checksum);
    return 0;
}

static void read_manual_words(uint16_t words[MIPSNES2_CPU_IO_JOYPAD_COUNT])
{
    unsigned bit;

    (void)memset(words, 0,
                 MIPSNES2_CPU_IO_JOYPAD_COUNT * sizeof(words[0]));
    for (bit = 0U; bit < 16U; ++bit) {
        uint8_t port1 = mipsnes2_snes_bus_read(
            &machine.bus, UINT32_C(0x004016));
        uint8_t port2 = mipsnes2_snes_bus_read(
            &machine.bus, UINT32_C(0x004017));

        words[0] = (uint16_t)((words[0] << 1U) | (port1 & 1U));
        words[2] = (uint16_t)((words[2] << 1U) | ((port1 >> 1U) & 1U));
        words[1] = (uint16_t)((words[1] << 1U) | (port2 & 1U));
        words[3] = (uint16_t)((words[3] << 1U) | ((port2 >> 1U) & 1U));
    }
}

static uint16_t read_auto_word(uint8_t index)
{
    uint32_t address = UINT32_C(0x004218) + (uint32_t)index * 2U;
    uint8_t low = mipsnes2_snes_bus_read(&machine.bus, address);
    uint8_t high = mipsnes2_snes_bus_read(&machine.bus, address + 1U);

    return (uint16_t)((uint16_t)low | ((uint16_t)high << 8U));
}

int main(void)
{
    static const uint16_t initial[MIPSNES2_CPU_IO_JOYPAD_COUNT] = {
        UINT16_C(0x9081), UINT16_C(0x7062),
        UINT16_C(0x5044), UINT16_C(0x3028)
    };
    const mipsnes2_master_clock auto_start =
        (mipsnes2_master_clock)225U * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS +
        MIPSNES2_HV_AUTO_JOYPAD_START_CLOCK;
    mipsnes2_machine invalid_machine;
    mipsnes2_machine_config config;
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu_timing_result timing_result;
    uint16_t manual[MIPSNES2_CPU_IO_JOYPAD_COUNT];
    size_t sequences;
    unsigned index;

    (void)memset(&invalid_machine, 0, sizeof(invalid_machine));
    CHECK(!mipsnes2_machine_set_joypad(NULL, 0U, 0U, false));
    CHECK(!mipsnes2_machine_set_joypad(
        &invalid_machine, 0U, 0U, false));
    CHECK(make_image() == 0);
    (void)memset(wram, 0, sizeof(wram));
    (void)memset(sram, 0, sizeof(sram));
    (void)memset(&config, 0, sizeof(config));
    config.image = rom;
    config.image_size = sizeof(rom);
    config.copier_policy = MIPSNES2_COPIER_HEADER_REJECT;
    config.wram = wram;
    config.wram_size = sizeof(wram);
    config.sram = sram;
    config.sram_capacity = sizeof(sram);
    config.standard = MIPSNES2_HV_STANDARD_NTSC;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    CHECK(!mipsnes2_machine_set_joypad(
        &machine, MIPSNES2_CPU_IO_JOYPAD_COUNT, 0U, false));
    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        CHECK(mipsnes2_machine_set_joypad(
            &machine, (uint8_t)index, initial[index], index != 2U));
    }

    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x004016), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x004016), 0U);
    read_manual_words(manual);
    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        CHECK(manual[index] == initial[index]);
    }
    CHECK((mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x004016)) &
           UINT8_C(3)) == UINT8_C(1));
    CHECK((mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x004017)) &
           UINT8_C(3)) == UINT8_C(3));

    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x004200), 1U);
    CHECK(mipsnes2_machine_run_until(
              &machine, auto_start, 100000U, 64U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.io.auto_joypad_busy);
    CHECK(mipsnes2_machine_set_joypad(
        &machine, 0U, UINT16_C(0xa55a), true));
    CHECK(mipsnes2_machine_run_until(
              &machine,
              auto_start + MIPSNES2_HV_AUTO_JOYPAD_DURATION_CLOCKS,
              100000U, 64U, &sequences, &report, &timing_result) ==
          MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(!machine.io.auto_joypad_busy);
    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        CHECK(read_auto_word((uint8_t)index) == initial[index]);
    }

    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        CHECK(machine.io.joypad_input[index] == 0U);
    }
    CHECK(machine.io.joypad_connected_mask == 0U);
    (void)puts("machine standard-joypad integration: OK");
    return 0;
}
