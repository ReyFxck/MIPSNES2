/* Original synthetic LoROM machine integration fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/machine/machine.h"
#include "core/scheduler/master_timing.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t rom[64 * 1024];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[8 * 1024];
static uint8_t vram[MIPSNES2_PPU_VRAM_SIZE];
static uint8_t cgram[MIPSNES2_PPU_CGRAM_SIZE];
static uint8_t oam[MIPSNES2_PPU_OAM_SIZE];
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
    mipsnes2_master_timing_report master_report;
    mipsnes2_master_clock oam_event;
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
    config.vram = vram;
    config.vram_size = sizeof(vram) - 1U;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_INVALID_ARGUMENT);
    config.vram_size = sizeof(vram);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115), UINT8_C(0x80));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), UINT8_C(0xff));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), UINT8_C(0x7f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002118), UINT8_C(0x4c));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002119), UINT8_C(0x3a));
    CHECK(vram[0xfffe] == UINT8_C(0x4c));
    CHECK(vram[0xffff] == UINT8_C(0x3a));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(vram[0xffff] == UINT8_C(0x3a));
    CHECK(machine.vram_ports.word_address == 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115), UINT8_C(0x80));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), UINT8_C(0xff));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), UINT8_C(0x7f));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002139)) == 0U);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213a)) == 0U);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002139)) ==
          UINT8_C(0x4c));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213a)) ==
          UINT8_C(0x3a));
    config.cgram = cgram;
    config.cgram_size = sizeof(cgram) - 1U;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_INVALID_ARGUMENT);
    config.cgram_size = sizeof(cgram);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002121), UINT8_C(0xff));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002122), UINT8_C(0x21));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002122), UINT8_C(0xfa));
    CHECK(cgram[0x1fe] == UINT8_C(0x21));
    CHECK(cgram[0x1ff] == UINT8_C(0x7a));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(cgram[0x1ff] == UINT8_C(0x7a));
    CHECK(machine.cgram_ports.address == 0U);
    config.standard = MIPSNES2_HV_STANDARD_NTSC;
    config.oam = oam;
    config.oam_size = sizeof(oam) - 1U;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_INVALID_ARGUMENT);
    config.oam_size = sizeof(oam);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x802101), UINT8_C(0xa5));
    CHECK(machine.oam_ports.obj_select == UINT8_C(0xa5));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002102), UINT8_C(0xff));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002103), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002104), UINT8_C(0x55));
    CHECK(oam[0x1fe] == 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002104), UINT8_C(0xaa));
    CHECK(oam[0x1fe] == UINT8_C(0x55) && oam[0x1ff] == UINT8_C(0xaa));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(oam[0x1ff] == UINT8_C(0xaa) && machine.oam_ports.byte_address == 0U);
    CHECK(machine.oam_ports.obj_select == 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002102), UINT8_C(0xff));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002138)) ==
          UINT8_C(0x55));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002138)) ==
          UINT8_C(0xaa));
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x200));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x802100), UINT8_C(0x0f));
    CHECK(!machine.ppu_forced_blank);
    CHECK(mipsnes2_scheduler_next_event_time(&machine.scheduler, &oam_event));
    CHECK(oam_event == MIPSNES2_HV_HBLANK_START_CLOCK);
    oam_event = (mipsnes2_master_clock)225U * 1364U + 40U;
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event - 1U, 300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x200));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event, 0U, &master_report) ==
          MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x200));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event, 1U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(master_report.events_dispatched == 1U);
    CHECK(machine.hv.v_counter == 225U && machine.hv.h_master_clock == 40U);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1fe));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002138)) ==
          UINT8_C(0x55));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x8f));
    CHECK(machine.ppu_forced_blank);
    oam_event = (mipsnes2_master_clock)225U * 1364U + 40U +
                (mipsnes2_master_clock)262U * 1364U - 4U;
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event, 300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1ff));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002121), UINT8_C(0xff));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213b)) ==
          UINT8_C(0x21));
    machine.counters.open_bus2 = UINT8_C(0x80);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213b)) ==
          UINT8_C(0xfa));
    CHECK(machine.counters.open_bus2 == UINT8_C(0xfa));
    CHECK(machine.cgram_ports.address == 0U);
    config.standard = MIPSNES2_HV_STANDARD_PAL;
    config.overscan = true;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    oam_event = (mipsnes2_master_clock)240U * 1364U + 40U;
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event, 300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.hv.v_counter == 240U && machine.hv.h_master_clock == 40U);
    oam_event = (mipsnes2_master_clock)(240U + 312U) * 1364U + 40U;
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002102), UINT8_C(0x0f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002103), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002104), UINT8_C(0x11));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002104), UINT8_C(0x22));
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x20));
    /* Forced blank release on the first VBlank line reloads immediately. */
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x802100), UINT8_C(0x0f));
    CHECK(!machine.ppu_forced_blank);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1e));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002138)) ==
          UINT8_C(0x11));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x0f));
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x8f));
    CHECK(machine.ppu_forced_blank &&
          machine.oam_ports.byte_address == UINT16_C(0x1f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              machine.scheduler.now + 1324U, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.hv.v_counter == 241U && machine.hv.h_master_clock == 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x0f));
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event, 300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1e));
    /* Status is prepared at HBlank for the next visible line. */
    (void)memset(oam, 0, sizeof(oam));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    for (unsigned i = 0U; i < 128U; ++i) {
        oam[4U * i] = UINT8_C(0x20);
        oam[4U * i + 1U] = i < 33U ? 0U : UINT8_C(0x80);
    }
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002138)) ==
          UINT8_C(0x20));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213e)) ==
          UINT8_C(0x21));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK - 1U, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_overflow == 0U);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 0U, &master_report) ==
          MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR);
    CHECK(machine.obj_overflow == 0U);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_line.range_count == 32U &&
          machine.obj_line.range_over && !machine.obj_line.time_over);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213e)) ==
          UINT8_C(0x61));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00213e), 0U);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213e)) ==
          UINT8_C(0x61));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x8f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              (mipsnes2_master_clock)312U * 1364U,
              300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_overflow == 0U);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213e)) ==
          UINT8_C(0x21));

    (void)memset(oam, 0, sizeof(oam));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    for (unsigned i = 0U; i < 128U; ++i) {
        oam[4U * i + 1U] = i < 5U ? 1U : UINT8_C(0x80);
        if (i < 5U) {
            oam[512U + i / 4U] |= (uint8_t)(2U << (2U * (i & 3U)));
        }
    }
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002101), UINT8_C(0x40));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK + 1364U - 1U,
              1U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_overflow == 0U);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK + 1364U,
              1U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_line.sliver_count == 34U &&
          machine.obj_line.time_over && !machine.obj_line.range_over);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x00213e)) ==
          UINT8_C(0x81));
    (void)puts("machine integration: OK");
    return 0;
}
