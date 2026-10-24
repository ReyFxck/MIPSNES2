/* Original synthetic LoROM machine integration fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/machine/machine.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t rom[64 * 1024];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[8 * 1024];
static mipsnes2_machine machine;

static void put16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static int make_image(void)
{
    mipsnes2_cartridge_image_view view;
    uint8_t *header = rom + MIPSNES2_LOROM_HEADER_OFFSET;
    uint16_t checksum;
    size_t mirrored;

    (void)memset(rom, 0, sizeof(rom));
    /* LDA #$5a; STA $0000; BRA -2. No external ROM or emulator source. */
    rom[0] = UINT8_C(0xa9);
    rom[1] = UINT8_C(0x5a);
    rom[2] = UINT8_C(0x8d);
    rom[3] = 0U;
    rom[4] = 0U;
    rom[5] = UINT8_C(0x80);
    rom[6] = UINT8_C(0xfe);
    (void)memcpy(header, "MIPSNES2 MACHINE TEST", MIPSNES2_INTERNAL_TITLE_SIZE);
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

int main(void)
{
    mipsnes2_machine_config config;
    mipsnes2_cpu_timing_report report;
    mipsnes2_master_clock end_reset;
    mipsnes2_cpu_timing_result timing_result;
    size_t sequences;

    CHECK(make_image() == 0);
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
    CHECK(mipsnes2_machine_resume(&machine, 1U, 0U, &report) ==
          MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.cpu.pc != UINT16_C(0x8000));
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.cpu.pc == UINT16_C(0x8000));
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(machine.hv.elapsed_master_clocks == machine.scheduler.now);
    end_reset = machine.scheduler.now;

    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.cpu.pc == UINT16_C(0x8002));
    CHECK(machine.cpu.a == UINT16_C(0x005a));
    CHECK(mipsnes2_machine_resume(&machine, machine.scheduler.now + 1U,
                                  0U, &report) ==
          MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(wram[0] == 0U);
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.cpu.pc == UINT16_C(0x8005));
    CHECK(wram[0] == UINT8_C(0x5a));
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.cpu.pc == UINT16_C(0x8005));
    CHECK(machine.scheduler.now > end_reset);
    CHECK(machine.hv.elapsed_master_clocks == machine.scheduler.now);
    CHECK(machine.bus.master_clocks == machine.scheduler.now);

    sram[0] = UINT8_C(0xa6);
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(machine.scheduler.now == 0U && machine.cpu.stopped);
    CHECK(wram[0] == UINT8_C(0x5a) && sram[0] == UINT8_C(0xa6));
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.cpu.pc == UINT16_C(0x8000));
    CHECK(mipsnes2_machine_run_until(
              &machine, machine.scheduler.now + UINT64_C(1000), 1000U,
              0U, &sequences, &report, &timing_result) ==
          MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(sequences > 0U && timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.scheduler.now == report.target_time && wram[0] == UINT8_C(0x5a));
    CHECK(mipsnes2_machine_run_until(
              &machine, UINT64_MAX, 1U, 0U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_SEQUENCE_LIMIT);
    CHECK(sequences == 1U && timing_result == MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_machine_run_until(
              &machine, UINT64_MAX, 0U, 0U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_INVALID_ARGUMENT);

    config.standard = (mipsnes2_hv_standard)2;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_INVALID_ARGUMENT);
    CHECK(!machine.ready);
    config.standard = MIPSNES2_HV_STANDARD_PAL;
    config.sram_capacity = 1U;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_CARTRIDGE_ERROR);
    config.sram_capacity = sizeof(sram);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    CHECK(machine.hv.standard == MIPSNES2_HV_STANDARD_PAL);
    (void)puts("machine integration: OK");
    return 0;
}
