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
static uint8_t aram[MIPSNES2_SMP_RAM_SIZE];
static uint8_t synthetic_ipl[MIPSNES2_SMP_IPL_SIZE];
static mipsnes2_pcm_frame pcm_frames[4];
static mipsnes2_machine machine;
static uint16_t captured_y[2];
static uint16_t captured_color[2];
static unsigned captured_count;

static void capture_line(void *context, uint16_t line_y,
                         const uint16_t pixels[256])
{
    unsigned *count = context;

    if (*count < 2U) {
        captured_y[*count] = line_y;
        captured_color[*count] = pixels[0];
    }
    ++*count;
}

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

    config.pcm_frames = pcm_frames;
    config.pcm_capacity = 0U;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_INVALID_ARGUMENT);
    config.pcm_capacity = 4U;
    CHECK(mipsnes2_machine_init(&machine, &config) ==
          MIPSNES2_MACHINE_INVALID_ARGUMENT);
    config.aram = aram;
    config.aram_size = sizeof(aram);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    CHECK(machine.dsp.pcm == &machine.pcm && machine.pcm.count == 0U);
    CHECK(mipsnes2_machine_run_until(
              &machine, UINT64_C(100000), 100000U, 0U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.dsp.sample_count > UINT64_C(4));
    CHECK(machine.pcm.count == 4U && machine.pcm.dropped_frames != 0U);
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(machine.dsp.pcm == &machine.pcm && machine.pcm.count == 0U &&
          machine.pcm.dropped_frames == 0U);
    config.pcm_frames = NULL;
    config.pcm_capacity = 0U;
    config.aram = NULL;
    config.aram_size = 0U;

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
    config.overscan = false;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002133), UINT8_C(0x04));
    CHECK(machine.ppu_setini == UINT8_C(0x04) && machine.hv.overscan);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002102), UINT8_C(0x0f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002103), 0U);
    (void)mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x002138));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x0f));
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              (mipsnes2_master_clock)225U * 1364U + 40U,
              300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1f));
    oam_event = (mipsnes2_master_clock)240U * 1364U + 40U;
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              oam_event, 300U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.hv.v_counter == 240U && machine.hv.h_master_clock == 40U);
    CHECK(machine.oam_ports.byte_address == UINT16_C(0x1e));
    oam_event = (mipsnes2_master_clock)(240U + 312U) * 1364U + 40U;
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(0x8f));
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
    (void)memset(oam, 0, sizeof(oam));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    for (unsigned i = 0U; i < 128U; ++i) {
        oam[4U * i + 1U] = i < 32U || i == 127U
                                  ? 0U : UINT8_C(0x80);
    }
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002102),
                             UINT8_C(0xfe));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002103),
                             UINT8_C(0x80));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_line.range_over &&
          machine.obj_line.range[0] == 127U &&
          machine.obj_line.range[31] == 30U);
    config.vram = vram;
    config.vram_size = sizeof(vram);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(oam, 0, sizeof(oam));
    (void)memset(vram, 0, sizeof(vram));
    for (unsigned i = 1U; i < 128U; ++i) {
        oam[4U * i + 1U] = UINT8_C(0x80);
    }
    vram[0] = UINT8_C(0x80);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.obj_pixels.pixels[0].color == 129U &&
          machine.obj_pixels.pixels[0].sprite_index == 0U &&
          machine.obj_pixels.pixels[1].color == 0U);
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    vram[0x402U * 2U] = 1U; /* BG1 screen map tile 1 at column 1. */
    vram[16] = UINT8_C(0x80);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210d), 16U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210d), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK - 1U, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(!machine.bg1_line.pixels[0].opaque);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.bg1_line.pixels[0].opaque &&
          machine.bg1_line.pixels[0].color == 1U);
    config.oam = oam;
    config.oam_size = sizeof(oam);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(oam, 0, sizeof(oam));
    (void)memset(vram, 0, sizeof(vram));
    for (unsigned i = 1U; i < 128U; ++i) {
        oam[4U * i + 1U] = UINT8_C(0x80);
    }
    oam[3] = UINT8_C(0x30); /* OBJ priority 3 above BG1. */
    cgram[258] = UINT8_C(0x1f); /* OBJ index 129: red. */
    cgram[259] = 0U;
    cgram[2] = UINT8_C(0xe0); /* BG index 1: green. */
    cgram[3] = UINT8_C(0x03);
    vram[0] = UINT8_C(0x80);
    vram[0x400U * 2U] = 2U;
    vram[32] = UINT8_C(0x80);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c),
                             UINT8_C(0x11));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212d), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002132),
                             UINT8_C(0x3f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002132),
                             UINT8_C(0x42));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002132),
                             UINT8_C(0x81));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.mode0_line.pixels[0].color == 129U &&
          machine.mode0_line.pixels[0].source == 5U);
    CHECK(machine.ppu_brightness == 15U &&
          machine.mode0_color_line.pixels[0] == UINT16_C(0x001f));
    CHECK(machine.fixed_color == UINT16_C(0x045f));
    CHECK(machine.sub_line.pixels[0].color == 1U &&
          machine.sub_line.pixels[0].source == 1U);
    CHECK(machine.sub_color_line.pixels[0] == UINT16_C(0x03e0));
    CHECK(machine.sub_color_line.pixels[1] == UINT16_C(0x045f));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    oam[3] = 0U;
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c),
                             UINT8_C(0x11));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x08));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.mode0_line.pixels[0].color == 1U &&
          machine.mode0_line.pixels[0].source == 1U);
    CHECK(machine.ppu_brightness == 8U &&
          machine.mode0_color_line.pixels[0] == UINT16_C(0x0200));
    CHECK(machine.mode0_line.pixels[0].color == 1U &&
          machine.mode0_line.pixels[0].source == 1U);
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002130), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002131), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002132),
                             UINT8_C(0x3f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x08));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.mode0_line.pixels[0].source == 1U);
    CHECK(machine.mode0_raw_line.pixels[0] == UINT16_C(0x03e0));
    CHECK(machine.mode0_color_line.pixels[0] == UINT16_C(0x0210));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002125),
                             UINT8_C(0x20)); /* Color window 1. */
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002126), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002127), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002130),
                             UINT8_C(0x80)); /* Clip main inside. */
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002131), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002132),
                             UINT8_C(0x3f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x08));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.color_window_inside[0] == 1U &&
          machine.color_window_inside[1] == 0U);
    CHECK(machine.mode0_color_line.pixels[0] == UINT16_C(0x0010));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c),
                             UINT8_C(0x11));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002123), 2U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002126), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002127), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212e), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212d), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212f), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002132),
                             UINT8_C(0x3f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.window_hidden[0] == 1U && machine.window_hidden[1] == 0U);
    CHECK(machine.mode0_line.pixels[0].color == 129U &&
          machine.mode0_line.pixels[0].source == 5U);
    CHECK(machine.mode0_color_line.pixels[0] == UINT16_C(0x001f));
    CHECK(machine.sub_window_hidden[0] == 1U);
    CHECK(machine.sub_line.pixels[0].source == 0U &&
          machine.sub_color_line.pixels[0] == UINT16_C(0x001f));
    config.oam = NULL;
    config.oam_size = 0U;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210d), 16U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210d), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100),
                             UINT8_C(0x0f));
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.bg1_line.pixels[0].opaque &&
          machine.bg1_line.pixels[0].color == 1U);
    /* Mode 1: 4bpp BG1/BG2, 2bpp BG3 and $2105 priority override. */
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    vram[0] = UINT8_C(0x80); /* BG1 4bpp color 1. */
    vram[0x2001] = UINT8_C(0x80); /* BG2 4bpp color 2. */
    vram[0x4000] = UINT8_C(0x80); /* BG3 2bpp color 3. */
    vram[0x4001] = UINT8_C(0x80);
    vram[0x800 * 2U + 1U] = UINT8_C(0x20); /* BG2 high priority. */
    vram[0xc00 * 2U + 1U] = UINT8_C(0x20); /* BG3 high priority. */
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002105), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002108), 8U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002109), 12U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210b), 0x10U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210c), 0x02U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 7U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), 15U);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.bg1_line.pixels[0].opaque &&
          machine.bg1_line.pixels[0].color == 1U);
    CHECK(machine.bg_other_lines[0].pixels[0].opaque &&
          machine.bg_other_lines[0].pixels[0].color == 2U);
    CHECK(machine.bg_other_lines[1].pixels[0].opaque &&
          machine.bg_other_lines[1].pixels[0].color == 3U);
    CHECK(machine.mode0_line.pixels[0].source == 2U &&
          machine.mode0_line.pixels[0].color == 2U);
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002105), 9U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002108), 8U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002109), 12U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210b), 0x10U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00210c), 0x02U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 7U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), 15U);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(machine.mode0_line.pixels[0].source == 3U &&
          machine.mode0_line.pixels[0].color == 3U);
    /* Optional video handoff copies each line before the next event. */
    config.video_line = capture_line;
    config.video_context = &captured_count;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    (void)memset(cgram, 0, sizeof(cgram));
    vram[0] = UINT8_C(0x80);
    cgram[2] = UINT8_C(0x1f);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), 4U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), 1U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), 15U);
    captured_count = 0U;
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK, 1U, &master_report) ==
          MIPSNES2_MASTER_TIMING_OK);
    CHECK(captured_count == 1U && captured_y[0] == 0U &&
          captured_color[0] == UINT16_C(0x001f));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), 0x8fU);
    CHECK(mipsnes2_master_timing_run_until(
              &machine.scheduler, &machine.hv, &machine.io,
              MIPSNES2_HV_HBLANK_START_CLOCK +
                  MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS,
              1U, &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(captured_count == 2U && captured_y[1] == 1U &&
          captured_color[1] == 0U &&
          captured_color[0] == UINT16_C(0x001f));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x004300)) ==
          UINT8_C(0xff));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x804373),
                            UINT8_C(0x9a));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x004373)) ==
          UINT8_C(0x9a));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00430b),
                            UINT8_C(0x47));
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x80430f)) ==
          UINT8_C(0x47));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x80420c),
                            UINT8_C(0xa5));
    CHECK(machine.bus.hdma_enable_mask == UINT8_C(0xa5));
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(mipsnes2_snes_bus_read(&machine.bus, UINT32_C(0x004373)) ==
          UINT8_C(0xff));
    /* CPU-triggered DMA reaches real shared VRAM, OAM and CGRAM ports. */
    config.oam = oam;
    config.oam_size = sizeof(oam);
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    machine.cpu.pc = 0U;
    machine.cpu.a = 7U;
    wram[0] = UINT8_C(0x8d);
    wram[1] = UINT8_C(0x0b);
    wram[2] = UINT8_C(0x42);
    wram[3] = UINT8_C(0xea);
    wram[0x1000] = UINT8_C(0x12);
    wram[0x1001] = UINT8_C(0x03);
    wram[0x1002] = UINT8_C(0x56);
    wram[0x1003] = UINT8_C(0x07);
    mipsnes2_snes_bus_write(&machine.bus, 0x2115U, UINT8_C(0x80));
    mipsnes2_snes_bus_write(&machine.bus, 0x2116U, 0U);
    mipsnes2_snes_bus_write(&machine.bus, 0x2117U, 0U);
    mipsnes2_snes_bus_write(&machine.bus, 0x2102U, 0U);
    mipsnes2_snes_bus_write(&machine.bus, 0x2103U, 0U);
    mipsnes2_snes_bus_write(&machine.bus, 0x2121U, 0U);
    {
        unsigned channel;
        const uint8_t modes[3] = {1U, 2U, 2U};
        const uint8_t ports[3] = {0x18U, 4U, 0x22U};
        for (channel = 0U; channel < 3U; ++channel) {
            uint8_t *state = machine.dma_registers.channel[channel];
            state[0] = modes[channel];
            state[1] = ports[channel];
            state[2] = 0U;
            state[3] = 0x10U;
            state[4] = 0x7eU;
            state[5] = 4U;
            state[6] = 0U;
        }
    }
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.bus.dma_request_mask == 7U);
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(report.dma_bytes == 12U && report.processor_cycles == 2U);
    CHECK(memcmp(vram, wram + 0x1000, 4U) == 0);
    CHECK(memcmp(oam, wram + 0x1000, 4U) == 0);
    CHECK(memcmp(cgram, wram + 0x1000, 4U) == 0);
    CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
    CHECK(machine.bus.dma_request_mask == 0U &&
          machine.bus.hdma_enable_mask == 0U && !machine.cursor.dma_active);
    /* Timed HDMA fetches a WRAM table and updates the real VRAM ports once at
     * each committed slot endpoint, including repeated horizon retries. */
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    wram[0x1000] = 1U;
    wram[0x1001] = UINT8_C(0xa5);
    wram[0x1002] = UINT8_C(0x6b);
    wram[0x1003] = 0U;
    machine.dma_registers.channel[0][0] = 1U;
    machine.dma_registers.channel[0][1] = UINT8_C(0x18);
    machine.dma_registers.channel[0][2] = 0U;
    machine.dma_registers.channel[0][3] = UINT8_C(0x10);
    machine.dma_registers.channel[0][4] = UINT8_C(0x7e);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115),
                            UINT8_C(0x80));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), 0U);
    CHECK(!machine.hdma_events_scheduled);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
    CHECK(machine.hdma_events_scheduled);
    CHECK(mipsnes2_machine_run_until(
              &machine, 100U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(machine.hdma_state.active_mask == 1U &&
          machine.dma_registers.channel[0][8] == 1U &&
          machine.dma_registers.channel[0][9] == UINT8_C(0x10) &&
          vram[0] == 0U);
    CHECK(mipsnes2_machine_run_until(
              &machine, MIPSNES2_HV_HBLANK_START_CLOCK + 16U,
              1000U, 8U, &sequences, &report, &timing_result) ==
          MIPSNES2_MACHINE_RUN_HORIZON);
    {
        mipsnes2_master_clock horizon;
        mipsnes2_master_clock byte_end;

        for (horizon = MIPSNES2_HV_HBLANK_START_CLOCK + 17U;
             horizon < MIPSNES2_HV_HBLANK_START_CLOCK + 110U;
             ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U, &sequences, &report,
                      &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
            if (machine.cursor.hdma_phase_pending &&
                machine.cursor.hdma_operation.kind ==
                    MIPSNES2_HDMA_OPERATION_TRANSFER) {
                break;
            }
        }
        CHECK(horizon < MIPSNES2_HV_HBLANK_START_CLOCK + 110U);
        byte_end = machine.cursor.hdma_end_time;
        CHECK(machine.scheduler.now < byte_end && vram[0] == 0U);
        CHECK(mipsnes2_machine_run_until(
                  &machine, byte_end - 1U, 1000U, 8U, &sequences, &report,
                  &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(vram[0] == 0U);
        CHECK(mipsnes2_machine_run_until(
                  &machine, byte_end - 1U, 1000U, 8U, &sequences, &report,
                  &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(vram[0] == 0U);
        CHECK(mipsnes2_machine_run_until(
                  &machine, byte_end, 1000U, 8U, &sequences, &report,
                  &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(vram[0] == UINT8_C(0xa5));
    }
    CHECK(mipsnes2_machine_run_until(
              &machine, 1300U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(vram[0] == UINT8_C(0xa5) && vram[1] == UINT8_C(0x6b) &&
          machine.hdma_state.terminated_mask == 1U &&
          machine.dma_registers.channel[0][8] == 4U);
    /* Indirect HDMA follows its separately loaded data-bank pointer through
     * the timed machine route before the next table descriptor terminates. */
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    wram[0x1000] = 1U;
    wram[0x1001] = 0U;
    wram[0x1002] = UINT8_C(0x20);
    wram[0x1003] = 0U;
    wram[0x1004] = 0U;
    wram[0x1005] = 0U;
    wram[0x2000] = UINT8_C(0xee);
    machine.dma_registers.channel[0][0] = UINT8_C(0x40);
    machine.dma_registers.channel[0][1] = UINT8_C(0x18);
    machine.dma_registers.channel[0][2] = 0U;
    machine.dma_registers.channel[0][3] = UINT8_C(0x10);
    machine.dma_registers.channel[0][4] = UINT8_C(0x7e);
    machine.dma_registers.channel[0][7] = UINT8_C(0x7e);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
    CHECK(mipsnes2_machine_run_until(
              &machine, 1300U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(vram[0] == UINT8_C(0xee) &&
          machine.hdma_state.terminated_mask == 1U &&
          machine.dma_registers.channel[0][8] == 5U &&
          machine.dma_registers.channel[0][5] == 0U &&
          machine.dma_registers.channel[0][6] == 0U &&
          machine.dma_registers.channel[0][9] == UINT8_C(0x10));
    /* A visible-line request pauses an in-flight general DMA between slots.
     * The unrelated general channel does not advance while HDMA owns the bus. */
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    (void)memset(wram + 0x2000, 0, 200U);
    wram[0x1000] = 1U;
    wram[0x1001] = UINT8_C(0xc7);
    wram[0x1002] = 0U;
    machine.dma_registers.channel[0][0] = 0U;
    machine.dma_registers.channel[0][1] = UINT8_C(0x18);
    machine.dma_registers.channel[0][2] = 0U;
    machine.dma_registers.channel[0][3] = UINT8_C(0x10);
    machine.dma_registers.channel[0][4] = UINT8_C(0x7e);
    machine.dma_registers.channel[1][0] = 0U;
    machine.dma_registers.channel[1][1] = UINT8_C(0x22);
    machine.dma_registers.channel[1][2] = 0U;
    machine.dma_registers.channel[1][3] = UINT8_C(0x20);
    machine.dma_registers.channel[1][4] = UINT8_C(0x7e);
    machine.dma_registers.channel[1][5] = 200U;
    machine.dma_registers.channel[1][6] = 0U;
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
    CHECK(mipsnes2_machine_run_until(
              &machine, 1000U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(machine.scheduler.now < MIPSNES2_HV_HBLANK_START_CLOCK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420b), 2U);
    CHECK(mipsnes2_machine_run_until(
              &machine, MIPSNES2_HV_HBLANK_START_CLOCK + 16U,
              1000U, 8U, &sequences, &report, &timing_result) ==
          MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(machine.cursor.dma_active);
    {
        mipsnes2_master_clock horizon;
        uint32_t paused_remaining;

        for (horizon = MIPSNES2_HV_HBLANK_START_CLOCK + 17U;
             horizon < MIPSNES2_HV_HBLANK_START_CLOCK + 80U;
             ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U, &sequences, &report,
                      &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
            if (machine.cursor.hdma_active) {
                break;
            }
        }
        CHECK(horizon < MIPSNES2_HV_HBLANK_START_CLOCK + 80U &&
              machine.cursor.dma_active && machine.cursor.hdma_active);
        paused_remaining = machine.cursor.dma_cursor.remaining;
        for (++horizon; horizon < MIPSNES2_HV_HBLANK_START_CLOCK + 120U;
             ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U, &sequences, &report,
                      &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
            CHECK(machine.cursor.dma_cursor.remaining == paused_remaining);
            if (!machine.cursor.hdma_active) {
                break;
            }
        }
        CHECK(!machine.cursor.hdma_active &&
              vram[0] == UINT8_C(0xc7));
    }
    /* HDMA on channel 0 ends that channel's in-flight general DMA after
     * the admitted byte. Channel 1 still runs after HDMA releases the bus. */
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    (void)memset(wram + 0x1000, 0, 256U);
    wram[0x1000] = 1U;
    wram[0x1001] = UINT8_C(0xc7);
    wram[0x1002] = 0U;
    wram[0x2000] = UINT8_C(0x93);
    machine.dma_registers.channel[0][0] = 0U;
    machine.dma_registers.channel[0][1] = UINT8_C(0x18);
    machine.dma_registers.channel[0][2] = 0U;
    machine.dma_registers.channel[0][3] = UINT8_C(0x10);
    machine.dma_registers.channel[0][4] = UINT8_C(0x7e);
    machine.dma_registers.channel[0][5] = 200U;
    machine.dma_registers.channel[0][6] = 0U;
    machine.dma_registers.channel[1][0] = 0U;
    machine.dma_registers.channel[1][1] = UINT8_C(0x18);
    machine.dma_registers.channel[1][2] = 0U;
    machine.dma_registers.channel[1][3] = UINT8_C(0x20);
    machine.dma_registers.channel[1][4] = UINT8_C(0x7e);
    machine.dma_registers.channel[1][5] = 1U;
    machine.dma_registers.channel[1][6] = 0U;
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
    CHECK(mipsnes2_machine_run_until(
              &machine, 1000U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420b), 3U);
    {
        mipsnes2_master_clock horizon;
        uint16_t cancelled_size;
        uint16_t cancelled_address;
        uint64_t dma_bytes;

        for (horizon = machine.scheduler.now + 1U; horizon < 1200U;
             ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U, &sequences, &report,
                      &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
            if (machine.cursor.hdma_active) {
                break;
            }
        }
        CHECK(horizon < 1200U && machine.cursor.dma_active &&
              machine.cursor.dma_cursor.pending_mask == 2U &&
              machine.cursor.dma_cursor.channel == 1U);
        cancelled_size = (uint16_t)machine.dma_registers.channel[0][5] |
                         ((uint16_t)machine.dma_registers.channel[0][6] << 8);
        cancelled_address =
            (uint16_t)machine.dma_registers.channel[0][2] |
            ((uint16_t)machine.dma_registers.channel[0][3] << 8);
        dma_bytes = machine.cursor.dma_bytes;
        CHECK(cancelled_size > 0U && cancelled_size < 200U);
        for (++horizon; horizon < 1250U; ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U, &sequences, &report,
                      &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
            if (!machine.cursor.hdma_active) {
                break;
            }
        }
        CHECK(horizon < 1250U && machine.cursor.dma_active &&
              machine.cursor.hdma_data_bytes == 1U &&
              machine.cursor.dma_bytes == dma_bytes);
        CHECK(mipsnes2_machine_run_until(
                  &machine, 1300U, 1000U, 8U, &sequences, &report,
                  &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(!machine.cursor.dma_active && !machine.cursor.hdma_active);
        CHECK(machine.dma_registers.channel[0][5] ==
                  (uint8_t)cancelled_size &&
              machine.dma_registers.channel[0][2] ==
                  (uint8_t)cancelled_address &&
              machine.dma_registers.channel[1][5] == 0U &&
              machine.dma_registers.channel[1][2] == 1U);
    }
    /* An injected early line request uses the same WRAM-refresh arbiter as
     * CPU and general DMA. No HDMA data effect leaks through the stall. */
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    (void)memset(vram, 0, sizeof(vram));
    wram[0x1000] = 1U;
    wram[0x1001] = UINT8_C(0xd4);
    wram[0x1002] = 0U;
    machine.dma_registers.channel[0][0] = 0U;
    machine.dma_registers.channel[0][1] = UINT8_C(0x18);
    machine.dma_registers.channel[0][2] = 0U;
    machine.dma_registers.channel[0][3] = UINT8_C(0x10);
    machine.dma_registers.channel[0][4] = UINT8_C(0x7e);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002115), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002116), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002117), 0U);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
    CHECK(mipsnes2_machine_run_until(
              &machine, 100U, 1000U, 0U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_TIMING_ERROR);
    CHECK(timing_result == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR &&
          machine.hdma_state.active_mask == 0U);
    CHECK(mipsnes2_machine_run_until(
              &machine, 520U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(machine.hdma_state.active_mask == 1U);
    machine.bus.hdma_line_pending = true;
    {
        mipsnes2_master_clock horizon;
        mipsnes2_master_clock stall_end;

        for (horizon = 521U; horizon < 580U; ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U, &sequences, &report,
                      &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
            if (machine.cursor.refresh_stall_active) {
                break;
            }
        }
        CHECK(horizon < 580U && machine.cursor.hdma_active);
        stall_end = machine.cursor.refresh_stall_end_time;
        CHECK(stall_end > machine.scheduler.now && vram[0] == 0U);
        CHECK(mipsnes2_machine_run_until(
                  &machine, stall_end - 1U, 1000U, 8U, &sequences,
                  &report, &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(report.refresh_master_clocks == 0U && vram[0] == 0U);
        CHECK(mipsnes2_machine_run_until(
                  &machine, stall_end, 1000U, 8U, &sequences,
                  &report, &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(report.refresh_master_clocks == 40U && vram[0] == 0U);
    }
    CHECK(mipsnes2_machine_run_until(
              &machine, 650U, 1000U, 8U, &sequences, &report,
              &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(vram[0] == UINT8_C(0xd4));
    /* Every active HDMA channel drops at the exact VBlank start, without
     * clearing $420C. The next frame may initialize it again. */
    {
        unsigned standard;

        config.vram = NULL;
        config.vram_size = 0U;
        config.cgram = NULL;
        config.cgram_size = 0U;
        config.oam = NULL;
        config.oam_size = 0U;
        config.video_line = NULL;
        config.video_context = NULL;
        for (standard = 0U; standard < 2U; ++standard) {
            mipsnes2_master_clock vblank_start;

            config.standard = standard == 0U
                ? MIPSNES2_HV_STANDARD_NTSC : MIPSNES2_HV_STANDARD_PAL;
            config.overscan = false;
            CHECK(mipsnes2_machine_init(&machine, &config) ==
                  MIPSNES2_MACHINE_OK);
            if (standard != 0U) {
                mipsnes2_snes_bus_write(&machine.bus,
                                        UINT32_C(0x002133), UINT8_C(0x04));
                CHECK(machine.ppu_setini == UINT8_C(0x04) &&
                      machine.hv.overscan);
            }
            wram[0x1000] = UINT8_C(0x80);
            wram[0x1001] = UINT8_C(0x24);
            wram[0x1002] = UINT8_C(0x80);
            wram[0x1003] = UINT8_C(0x25);
            machine.dma_registers.channel[0][0] = 0U;
            machine.dma_registers.channel[0][1] = UINT8_C(0x18);
            machine.dma_registers.channel[0][2] = 0U;
            machine.dma_registers.channel[0][3] = UINT8_C(0x10);
            machine.dma_registers.channel[0][4] = UINT8_C(0x7e);
            mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
            vblank_start =
                (mipsnes2_master_clock)mipsnes2_hv_vblank_start_line(
                    &machine.hv) * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
            CHECK(mipsnes2_machine_run_until(
                      &machine, vblank_start - 1U, 100000U, 8U,
                      &sequences, &report, &timing_result) ==
                  MIPSNES2_MACHINE_RUN_HORIZON);
            CHECK(machine.hdma_state.active_mask == 1U &&
                  machine.bus.hdma_enable_mask == 1U);
            CHECK(mipsnes2_machine_run_until(
                      &machine, vblank_start, 1000U, 0U,
                      &sequences, &report, &timing_result) ==
                  MIPSNES2_MACHINE_RUN_TIMING_ERROR);
            CHECK(timing_result == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR &&
                  machine.hdma_state.active_mask == 1U);
            CHECK(mipsnes2_machine_run_until(
                      &machine, vblank_start, 1000U, 8U,
                      &sequences, &report, &timing_result) ==
                  MIPSNES2_MACHINE_RUN_HORIZON);
            CHECK(machine.hdma_state.active_mask == 0U &&
                  machine.hdma_state.transfer_mask == 0U &&
                  machine.bus.hdma_enable_mask == 1U &&
                  !machine.bus.hdma_line_pending);
        }
    }
    /* Enabling HDMA during the extra overscan lines still schedules the
     * current frame's V=240 deactivation rather than skipping a frame. */
    {
        mipsnes2_master_clock enable_time =
            UINT64_C(230) * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
        mipsnes2_master_clock vblank_start =
            UINT64_C(240) * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
        uint8_t *state;

        config.standard = MIPSNES2_HV_STANDARD_PAL;
        config.overscan = false;
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_OK);
        CHECK(machine.hdma_state.initialized &&
              machine.hdma_state.registers == &machine.dma_registers);
        mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002133),
                                UINT8_C(0x04));
        CHECK(mipsnes2_machine_run_until(
                  &machine, enable_time, 100000U, 8U,
                  &sequences, &report, &timing_result) ==
              MIPSNES2_MACHINE_RUN_HORIZON);
        state = machine.dma_registers.channel[0];
        state[0] = 0U;
        state[1] = UINT8_C(0x18);
        state[4] = UINT8_C(0x7e);
        state[8] = 0U;
        state[9] = UINT8_C(0x10);
        state[10] = UINT8_C(0x8f);
        (void)memset(wram + 0x1000, UINT8_C(0x5a), 16U);
        mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
        machine.hdma_state.active_mask = 1U;
        machine.hdma_state.transfer_mask = 1U;
        CHECK(mipsnes2_master_timing_run_until(
                  &machine.scheduler, &machine.hv, &machine.io,
                  vblank_start - 1U, 32U, &master_report) ==
              MIPSNES2_MASTER_TIMING_OK);
        CHECK(machine.hdma_state.active_mask == 1U && machine.ready);
        CHECK(mipsnes2_master_timing_run_until(
                  &machine.scheduler, &machine.hv, &machine.io,
                  vblank_start, 1U, &master_report) ==
              MIPSNES2_MASTER_TIMING_OK);
        CHECK(machine.hdma_state.active_mask == 0U &&
              machine.hdma_state.transfer_mask == 0U && machine.ready);
    }
    /* Eight four-byte channels overrun the 252 clocks between the last
     * visible-line request and VBlank. VBlank waits for the admitted slot,
     * then stops the rest of that HDMA line without stale snapshots. */
    {
        mipsnes2_master_clock vblank_start =
            225U * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
        mipsnes2_master_clock horizon;
        unsigned channel;

        config.standard = MIPSNES2_HV_STANDARD_NTSC;
        config.overscan = false;
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_OK);
        (void)memset(wram + 0x1000, 0xff, 0x8000);
        for (channel = 0U; channel < 8U; ++channel) {
            uint8_t *state = machine.dma_registers.channel[channel];
            uint16_t table = (uint16_t)(0x1000U + channel * 0x1000U);

            state[0] = 4U;
            state[1] = UINT8_C(0x18);
            state[2] = (uint8_t)table;
            state[3] = (uint8_t)(table >> 8);
            state[4] = UINT8_C(0x7e);
        }
        mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c),
                                UINT8_C(0xff));
        CHECK(mipsnes2_machine_run_until(
                  &machine, vblank_start - 1U, 100000U, 8U,
                  &sequences, &report, &timing_result) ==
              MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(machine.cursor.hdma_active &&
              machine.hdma_state.active_mask == UINT8_C(0xff));
        CHECK(mipsnes2_machine_run_until(
                  &machine, vblank_start, 1000U, 8U,
                  &sequences, &report, &timing_result) ==
              MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(machine.cursor.hdma_active &&
              machine.bus.hdma_vblank_pending);
        for (horizon = vblank_start + 1U;
             horizon < vblank_start + 50U; ++horizon) {
            CHECK(mipsnes2_machine_run_until(
                      &machine, horizon, 1000U, 8U,
                      &sequences, &report, &timing_result) ==
                  MIPSNES2_MACHINE_RUN_HORIZON);
            if (!machine.cursor.hdma_active) {
                break;
            }
        }
        CHECK(horizon < vblank_start + 50U &&
              machine.hdma_state.active_mask == 0U &&
              machine.hdma_state.transfer_mask == 0U &&
              !machine.bus.hdma_vblank_pending &&
              machine.bus.hdma_enable_mask == UINT8_C(0xff));
    }
    /* Enabling in VBlank schedules initialization in the next frame for
     * both scanline standards, including NTSC's short current frame. */
    {
        unsigned standard;

        config.vram = NULL;
        config.vram_size = 0U;
        config.cgram = NULL;
        config.cgram_size = 0U;
        config.oam = NULL;
        config.oam_size = 0U;
        config.video_line = NULL;
        config.video_context = NULL;
        config.overscan = false;
        for (standard = 0U; standard < 2U; ++standard) {
            mipsnes2_master_clock next_frame =
                (standard == 0U ? MIPSNES2_HV_NTSC_SCANLINES
                                : MIPSNES2_HV_PAL_SCANLINES) *
                MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                (standard == 0U ? 4U : 0U);
            mipsnes2_master_clock vblank_time =
                225U * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + 100U;

            config.standard = standard == 0U
                ? MIPSNES2_HV_STANDARD_NTSC : MIPSNES2_HV_STANDARD_PAL;
            CHECK(mipsnes2_machine_init(&machine, &config) ==
                  MIPSNES2_MACHINE_OK);
            CHECK(mipsnes2_master_timing_run_until(
                      &machine.scheduler, &machine.hv, &machine.io,
                      vblank_time, 0U, &master_report) ==
                  MIPSNES2_MASTER_TIMING_OK);
            mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00420c), 1U);
            CHECK(mipsnes2_master_timing_run_until(
                      &machine.scheduler, &machine.hv, &machine.io,
                      next_frame + 23U, 1U, &master_report) ==
                  MIPSNES2_MASTER_TIMING_OK);
            CHECK(!machine.bus.hdma_frame_pending);
            CHECK(mipsnes2_master_timing_run_until(
                      &machine.scheduler, &machine.hv, &machine.io,
                      next_frame + 24U, 1U, &master_report) ==
                  MIPSNES2_MASTER_TIMING_OK);
            CHECK(machine.bus.hdma_frame_pending &&
                  !machine.bus.hdma_line_pending);
            CHECK(mipsnes2_master_timing_run_until(
                      &machine.scheduler, &machine.hv, &machine.io,
                      next_frame + 1112U, 1U, &master_report) ==
                  MIPSNES2_MASTER_TIMING_OK);
            CHECK(machine.bus.hdma_line_pending);
        }
    }
    /* A SETINI overscan write after the normal VBlank boundary is retained
     * without moving the active frame's boundary. It takes effect at the
     * next frame start, including when no optional PPU memory is attached. */
    {
        mipsnes2_master_clock next_frame =
            (mipsnes2_master_clock)MIPSNES2_HV_NTSC_SCANLINES *
                MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS - 4U;
        mipsnes2_master_clock normal_vblank =
            UINT64_C(225) * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
        mipsnes2_master_clock overscan_vblank =
            UINT64_C(240) * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;

        config.standard = MIPSNES2_HV_STANDARD_NTSC;
        config.overscan = false;
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_OK);
        CHECK(mipsnes2_master_timing_run_until(
                  &machine.scheduler, &machine.hv, &machine.io,
                  normal_vblank + 100U, 0U, &master_report) ==
              MIPSNES2_MASTER_TIMING_OK);
        CHECK(machine.io.vblank && !machine.hv.overscan);
        mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002133),
                                UINT8_C(0x04));
        CHECK(machine.ppu_setini == UINT8_C(0x04) &&
              !machine.hv.overscan && machine.io.vblank);
        CHECK(mipsnes2_master_timing_run_until(
                  &machine.scheduler, &machine.hv, &machine.io,
                  next_frame, 1U, &master_report) ==
              MIPSNES2_MASTER_TIMING_OK);
        CHECK(machine.hv.v_counter == 0U &&
              machine.hv.h_master_clock == 0U && machine.hv.overscan &&
              !machine.io.vblank);
        CHECK(mipsnes2_master_timing_run_until(
                  &machine.scheduler, &machine.hv, &machine.io,
                  next_frame + normal_vblank, 0U, &master_report) ==
              MIPSNES2_MASTER_TIMING_OK);
        CHECK(!machine.io.vblank);
        CHECK(mipsnes2_master_timing_run_until(
                  &machine.scheduler, &machine.hv, &machine.io,
                  next_frame + overscan_vblank, 0U, &master_report) ==
              MIPSNES2_MASTER_TIMING_OK);
        CHECK(machine.io.vblank);
    }
    /* With no external APU callback, the machine owns the directional
     * $2140-$2143 mailbox and exposes its S-SMP side to the future core. */
    {
        uint8_t value = 0U;

        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_OK);
        mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x802174),
                                UINT8_C(0xa6));
        CHECK(mipsnes2_apu_io_ports_smp_read(&machine.apu_ports, 0U,
                                             &value));
        CHECK(value == UINT8_C(0xa6));
        CHECK(mipsnes2_apu_io_ports_smp_write(&machine.apu_ports, 2U,
                                              UINT8_C(0x5c)));
        CHECK(mipsnes2_snes_bus_read(&machine.bus,
                                     UINT32_C(0x002142)) == UINT8_C(0x5c));
        CHECK(machine.apu_ports.cpu_to_apu[2] == 0U &&
              machine.apu_ports.apu_to_cpu[0] == 0U);
        CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
        CHECK(machine.apu_ports.cpu_to_apu[0] == 0U &&
              machine.apu_ports.apu_to_cpu[2] == 0U);
    }
    /* Optional S-SMP map borrows ARAM and synthetic IPL bytes. The CPU bus
     * and S-SMP map meet only through the machine-owned directional mailbox. */
    {
        uint8_t value = 0U;
        mipsnes2_spc700_registers smp_registers = {0};
        unsigned apu_attempts;

        (void)memset(aram, UINT8_C(0x5a), sizeof(aram));
        (void)memset(synthetic_ipl, UINT8_C(0x31), sizeof(synthetic_ipl));
        config.aram = aram;
        config.aram_size = sizeof(aram) - 1U;
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_INVALID_ARGUMENT);
        config.aram_size = sizeof(aram);
        config.smp_ipl = synthetic_ipl;
        config.smp_ipl_size = sizeof(synthetic_ipl) - 1U;
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_INVALID_ARGUMENT);
        config.smp_ipl_size = sizeof(synthetic_ipl);
        config.apu = mipsnes2_apu_io_ports_make_cpu_interface(
            &machine.apu_ports);
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_INVALID_ARGUMENT);
        (void)memset(&config.apu, 0, sizeof(config.apu));
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_OK);
        CHECK(machine.smp_memory.ipl_enabled);
        CHECK(mipsnes2_smp_memory_write(&machine.smp_memory,
                                        UINT16_C(0x00fc), UINT8_C(1)));
        CHECK(mipsnes2_smp_memory_write(&machine.smp_memory,
                                        UINT16_C(0x00f1), UINT8_C(0x84)));
        CHECK(mipsnes2_smp_memory_advance(&machine.smp_memory, 16U));
        CHECK(mipsnes2_smp_memory_read(&machine.smp_memory,
                                       UINT16_C(0x00ff), &value));
        CHECK(value == 1U);
        mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x802177),
                                UINT8_C(0xa4));
        CHECK(mipsnes2_smp_memory_read(&machine.smp_memory,
                                       UINT16_C(0x00f7), &value));
        CHECK(value == UINT8_C(0xa4));
        CHECK(mipsnes2_smp_memory_write(&machine.smp_memory,
                                        UINT16_C(0x00f5), UINT8_C(0xc2)));
        CHECK(mipsnes2_snes_bus_read(&machine.bus,
                                     UINT32_C(0x002141)) == UINT8_C(0xc2));
        CHECK(mipsnes2_smp_memory_read(&machine.smp_memory,
                                       UINT16_C(0xffc0), &value));
        CHECK(value == UINT8_C(0x31));
        CHECK(mipsnes2_smp_memory_write(&machine.smp_memory,
                                        UINT16_C(0xffc0), UINT8_C(0x7d)));
        CHECK(mipsnes2_smp_memory_write(&machine.smp_memory,
                                        UINT16_C(0x00f1), UINT8_C(0x20)));
        CHECK(machine.apu_ports.cpu_to_apu[3] == 0U &&
              machine.apu_ports.apu_to_cpu[1] == UINT8_C(0xc2));
        CHECK(mipsnes2_smp_memory_read(&machine.smp_memory,
                                       UINT16_C(0xffc0), &value));
        CHECK(value == UINT8_C(0x7d));
        CHECK(mipsnes2_machine_power_on(&machine) == MIPSNES2_MACHINE_OK);
        CHECK(machine.smp_memory.ipl_enabled &&
              aram[0xffc0] == UINT8_C(0x7d) &&
              machine.smp_memory.timers.enabled_mask == 0U &&
              machine.smp_memory.timers.stage1_phase == 0U);
        CHECK(machine.apu_ports.apu_to_cpu[1] == 0U);

        /* An explicit caller seed starts the SPC700 on the synthetic IPL.
         * The machine's normal 5A22 resume also catches the APU clock up. */
        synthetic_ipl[0] = UINT8_C(0xe8); /* MOV A,#$0c */
        synthetic_ipl[1] = UINT8_C(0x0c);
        synthetic_ipl[2] = UINT8_C(0xc4); /* MOV $F2,A */
        synthetic_ipl[3] = UINT8_C(0xf2);
        synthetic_ipl[4] = UINT8_C(0xe8); /* MOV A,#$44 */
        synthetic_ipl[5] = UINT8_C(0x44);
        synthetic_ipl[6] = UINT8_C(0xc4); /* MOV $F3,A */
        synthetic_ipl[7] = UINT8_C(0xf3);
        synthetic_ipl[8] = UINT8_C(0xef); /* SLEEP */
        smp_registers.pc = UINT16_C(0xffc0);
        config.smp_initial_registers = &smp_registers;
        CHECK(mipsnes2_machine_init(&machine, &config) ==
              MIPSNES2_MACHINE_OK);
        smp_registers.pc = 0U; /* Configuration was copied, not borrowed. */
        CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 0U, &report) ==
              MIPSNES2_CPU_TIMING_OK);
        for (apu_attempts = 0U;
             apu_attempts < 64U &&
             machine.smp_cpu.wait_state == MIPSNES2_SPC700_RUNNING;
             ++apu_attempts) {
            CHECK(mipsnes2_machine_resume(
                      &machine, UINT64_MAX, 0U, &report) ==
                  MIPSNES2_CPU_TIMING_OK);
        }
        CHECK(machine.apu_timing.master_clocks == machine.scheduler.now);
        CHECK(machine.smp_cpu.wait_state == MIPSNES2_SPC700_SLEEPING);
        CHECK(machine.dsp.registers[UINT8_C(0x0c)] == UINT8_C(0x44));
        CHECK(machine.smp_memory.dsp == &machine.dsp);
    }
    (void)puts("machine integration: OK");
    return 0;
}
