/* Integrated machine SRAM persistence-handoff fixture. See LICENSE. */
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
    /* LDA #$51; STA $70:0003; LDA #$52; STA $70:0003; BRA -2. */
    rom[0] = UINT8_C(0xa9);
    rom[1] = UINT8_C(0x51);
    rom[2] = UINT8_C(0x8f);
    rom[3] = UINT8_C(0x03);
    rom[4] = UINT8_C(0x00);
    rom[5] = UINT8_C(0x70);
    rom[6] = UINT8_C(0xa9);
    rom[7] = UINT8_C(0x52);
    rom[8] = UINT8_C(0x8f);
    rom[9] = UINT8_C(0x03);
    rom[10] = UINT8_C(0x00);
    rom[11] = UINT8_C(0x70);
    rom[12] = UINT8_C(0x80);
    rom[13] = UINT8_C(0xfe);
    (void)memcpy(header, "MIPSNES2 SRAM TEST   ", MIPSNES2_INTERNAL_TITLE_SIZE);
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

static int resume_sequence(void)
{
    mipsnes2_cpu_timing_report report;

    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    return 0;
}

int main(void)
{
    mipsnes2_machine invalid_machine;
    mipsnes2_machine_config config;
    mipsnes2_cartridge_sram_view first;
    mipsnes2_cartridge_sram_view current;

    (void)memset(&invalid_machine, 0, sizeof(invalid_machine));
    CHECK(!mipsnes2_machine_sram_view_get(NULL, &current));
    CHECK(!mipsnes2_machine_sram_view_get(&invalid_machine, &current));
    CHECK(!mipsnes2_machine_sram_acknowledge(NULL, 0U));
    CHECK(!mipsnes2_machine_sram_acknowledge(&invalid_machine, 0U));
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
    CHECK(!mipsnes2_machine_sram_view_get(&machine, NULL));
    CHECK(mipsnes2_machine_sram_view_get(&machine, &current));
    CHECK(current.data == sram && current.size == sizeof(sram));
    CHECK(current.revision == 0U && !current.dirty);

    CHECK(resume_sequence() == 0); /* RESET */
    CHECK(resume_sequence() == 0); /* LDA #$51 */
    CHECK(resume_sequence() == 0); /* STA $70:0003 */
    CHECK(mipsnes2_machine_sram_view_get(&machine, &first));
    CHECK(first.data[3] == UINT8_C(0x51));
    CHECK(first.revision == UINT64_C(1) && first.dirty);

    CHECK(resume_sequence() == 0); /* LDA #$52 */
    CHECK(resume_sequence() == 0); /* STA $70:0003 */
    CHECK(mipsnes2_machine_sram_view_get(&machine, &current));
    CHECK(current.data[3] == UINT8_C(0x52));
    CHECK(current.revision == UINT64_C(2) && current.dirty);
    CHECK(!mipsnes2_machine_sram_acknowledge(&machine, first.revision));
    CHECK(mipsnes2_machine_sram_acknowledge(&machine, current.revision));
    CHECK(mipsnes2_machine_sram_view_get(&machine, &current));
    CHECK(current.revision == UINT64_C(2) && !current.dirty);

    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(mipsnes2_machine_sram_view_get(&machine, &current));
    CHECK(current.data[3] == UINT8_C(0x52));
    CHECK(current.revision == UINT64_C(2) && !current.dirty);
    (void)puts("machine SRAM persistence handoff: OK");
    return 0;
}
