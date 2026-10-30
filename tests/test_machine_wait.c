/* Integrated machine WAI idle/wake regression fixture. See LICENSE. */
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

static int finish_image(const char title[MIPSNES2_INTERNAL_TITLE_SIZE])
{
    mipsnes2_cartridge_image_view view;
    uint8_t *header = rom + MIPSNES2_LOROM_HEADER_OFFSET;
    uint16_t checksum;
    size_t mirrored;

    (void)memcpy(header, title, MIPSNES2_INTERNAL_TITLE_SIZE);
    header[0x15] = UINT8_C(0x20);
    header[0x16] = UINT8_C(0x02);
    header[0x17] = UINT8_C(6);
    header[0x18] = UINT8_C(3);
    put16(header + 0x3a, UINT16_C(0x8010));
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

static int make_nmi_wait_image(void)
{
    (void)memset(rom, 0, sizeof(rom));
    rom[0x00] = UINT8_C(0xa9); /* LDA #$80 */
    rom[0x01] = UINT8_C(0x80);
    rom[0x02] = UINT8_C(0x8d); /* STA $4200 */
    rom[0x03] = UINT8_C(0x00);
    rom[0x04] = UINT8_C(0x42);
    rom[0x05] = UINT8_C(0xcb); /* WAI */
    rom[0x06] = UINT8_C(0xa9); /* LDA #$5a */
    rom[0x07] = UINT8_C(0x5a);
    rom[0x08] = UINT8_C(0x8d); /* STA $0000 */
    rom[0x09] = UINT8_C(0x00);
    rom[0x0a] = UINT8_C(0x00);
    rom[0x0b] = UINT8_C(0xdb); /* STP */
    rom[0x10] = UINT8_C(0xa9); /* NMI: LDA #$a5 */
    rom[0x11] = UINT8_C(0xa5);
    rom[0x12] = UINT8_C(0x8d); /* STA $0001 */
    rom[0x13] = UINT8_C(0x01);
    rom[0x14] = UINT8_C(0x00);
    rom[0x15] = UINT8_C(0x40); /* RTI */
    return finish_image("MIPSNES2 WAI NMI     ");
}

static int make_single_opcode_image(uint8_t opcode, const char *title)
{
    char padded[MIPSNES2_INTERNAL_TITLE_SIZE];
    size_t length = strlen(title);

    (void)memset(rom, 0, sizeof(rom));
    (void)memset(padded, ' ', sizeof(padded));
    if (length > sizeof(padded)) {
        length = sizeof(padded);
    }
    (void)memcpy(padded, title, length);
    rom[0] = opcode;
    return finish_image(padded);
}

static int make_irq_wait_image(void)
{
    (void)memset(rom, 0, sizeof(rom));
    rom[0x00] = UINT8_C(0xa9); /* LDA #$64 */
    rom[0x01] = UINT8_C(0x64);
    rom[0x02] = UINT8_C(0x8d); /* STA $4207 */
    rom[0x03] = UINT8_C(0x07);
    rom[0x04] = UINT8_C(0x42);
    rom[0x05] = UINT8_C(0x9c); /* STZ $4208 */
    rom[0x06] = UINT8_C(0x08);
    rom[0x07] = UINT8_C(0x42);
    rom[0x08] = UINT8_C(0xa9); /* LDA #$10 */
    rom[0x09] = UINT8_C(0x10);
    rom[0x0a] = UINT8_C(0x8d); /* STA $4200 */
    rom[0x0b] = UINT8_C(0x00);
    rom[0x0c] = UINT8_C(0x42);
    rom[0x0d] = UINT8_C(0xcb); /* WAI */
    rom[0x0e] = UINT8_C(0xa9); /* LDA #$33 */
    rom[0x0f] = UINT8_C(0x33);
    rom[0x10] = UINT8_C(0x8d); /* STA $0002 */
    rom[0x11] = UINT8_C(0x02);
    rom[0x12] = UINT8_C(0x00);
    rom[0x13] = UINT8_C(0xdb); /* STP */
    return finish_image("MIPSNES2 WAI IRQ     ");
}

static int init_machine(void)
{
    mipsnes2_machine_config config;

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
    return 0;
}

static int resume_ok(mipsnes2_cpu_timing_report *report)
{
    CHECK(mipsnes2_machine_resume(&machine, UINT64_MAX, 1U, report) ==
          MIPSNES2_CPU_TIMING_OK);
    return 0;
}

static int test_exact_nmi_wake(void)
{
    const mipsnes2_master_clock nmi_time =
        (mipsnes2_master_clock)225U *
            MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS +
        MIPSNES2_HV_NMI_FLAG_CLOCK;
    mipsnes2_cpu_timing_report report;

    CHECK(make_nmi_wait_image() == 0);
    CHECK(init_machine() == 0);
    CHECK(resume_ok(&report) == 0); /* RESET */
    CHECK(machine.cpu.pc == UINT16_C(0x8000));
    CHECK(resume_ok(&report) == 0); /* LDA #$80 */
    CHECK(resume_ok(&report) == 0); /* STA $4200 */
    CHECK(machine.io.nmitimen == UINT8_C(0x80));
    CHECK(resume_ok(&report) == 0); /* WAI */
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(machine.cpu.waiting && machine.cpu.pc == UINT16_C(0x8006));
    CHECK(machine.scheduler.now < nmi_time - 1U);

    CHECK(mipsnes2_machine_resume(&machine, nmi_time - 1U, 1U, &report) ==
          MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.scheduler.now == nmi_time - 1U);
    CHECK(machine.hv.v_counter == UINT16_C(225));
    CHECK(machine.hv.h_master_clock == UINT16_C(1));
    CHECK(machine.cpu.waiting && !report.nmi_delivered);
    CHECK(report.cpu_result == MIPSNES2_CPU_WAITING);

    CHECK(mipsnes2_machine_resume(&machine, nmi_time, 1U, &report) ==
          MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.scheduler.now == nmi_time);
    CHECK(machine.hv.h_master_clock == MIPSNES2_HV_NMI_FLAG_CLOCK);
    CHECK(report.nmi_delivered && !machine.cpu.waiting);
    CHECK(machine.cursor.active);

    CHECK(resume_ok(&report) == 0); /* complete NMI entry */
    CHECK(machine.cpu.pc == UINT16_C(0x8010));
    CHECK(resume_ok(&report) == 0); /* LDA #$a5 */
    CHECK(resume_ok(&report) == 0); /* STA $0001 */
    CHECK(wram[1] == UINT8_C(0xa5));
    CHECK(resume_ok(&report) == 0); /* RTI */
    CHECK(machine.cpu.pc == UINT16_C(0x8006));
    CHECK(resume_ok(&report) == 0); /* LDA #$5a */
    CHECK(resume_ok(&report) == 0); /* STA $0000 */
    CHECK(wram[0] == UINT8_C(0x5a));
    return 0;
}

static int test_wait_reaches_horizon(void)
{
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu_timing_result timing_result;
    size_t sequences;

    CHECK(make_single_opcode_image(UINT8_C(0xcb),
                                   "MIPSNES2 WAI IDLE") == 0);
    CHECK(init_machine() == 0);
    CHECK(mipsnes2_machine_run_until(
              &machine, UINT64_C(5000), 100U, 1U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(sequences == 2U);
    CHECK(machine.scheduler.now == UINT64_C(5000));
    CHECK(machine.hv.elapsed_master_clocks == UINT64_C(5000));
    CHECK(machine.cpu.waiting && report.cpu_result == MIPSNES2_CPU_WAITING);

    /* A no-channel HDMA wake boundary performs no CPU sequence and must not
     * consume the sole sequence slot before idle reaches the horizon. */
    machine.bus.hdma_frame_pending = true;
    CHECK(mipsnes2_machine_run_until(
              &machine, UINT64_C(7000), 1U, 1U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_HORIZON);
    CHECK(sequences == 0U && machine.scheduler.now == UINT64_C(7000));
    CHECK(!machine.bus.hdma_frame_pending);
    CHECK(machine.cpu.waiting && !machine.cursor.active);
    return 0;
}

static int test_masked_timer_irq_wake(void)
{
    const mipsnes2_master_clock irq_time = UINT64_C(14) + UINT64_C(100) * 4U;
    mipsnes2_cpu_timing_report report;

    CHECK(make_irq_wait_image() == 0);
    CHECK(init_machine() == 0);
    CHECK(resume_ok(&report) == 0); /* RESET */
    CHECK(resume_ok(&report) == 0); /* LDA #$64 */
    CHECK(resume_ok(&report) == 0); /* STA $4207 */
    CHECK(resume_ok(&report) == 0); /* STZ $4208 */
    CHECK(resume_ok(&report) == 0); /* LDA #$10 */
    CHECK(resume_ok(&report) == 0); /* STA $4200 */
    CHECK(machine.io.htime == UINT16_C(100));
    CHECK(machine.io.nmitimen == UINT8_C(0x10));
    CHECK(resume_ok(&report) == 0); /* WAI */
    CHECK(machine.cpu.waiting && machine.scheduler.now < irq_time - 1U);
    CHECK((machine.cpu.p & MIPSNES2_CPU_FLAG_I) != 0U);

    CHECK(mipsnes2_machine_resume(&machine, irq_time - 1U, 1U, &report) ==
          MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.cpu.waiting && !report.irq_level_delivered);
    CHECK(machine.hv.h_master_clock == UINT16_C(413));
    CHECK(mipsnes2_machine_resume(&machine, irq_time, 1U, &report) ==
          MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(machine.scheduler.now == irq_time);
    CHECK(machine.hv.h_master_clock == UINT16_C(414));
    CHECK(report.irq_level_delivered);
    CHECK(!machine.cpu.waiting && machine.cpu.pc == UINT16_C(0x800e));
    CHECK(machine.cursor.active);

    CHECK(resume_ok(&report) == 0); /* LDA #$33, not IRQ entry */
    CHECK(machine.cpu.pc == UINT16_C(0x8010));
    CHECK(resume_ok(&report) == 0); /* STA $0002 */
    CHECK(wram[2] == UINT8_C(0x33));
    return 0;
}

static int test_stp_remains_cpu_stop(void)
{
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu_timing_result timing_result;
    size_t sequences;

    CHECK(make_single_opcode_image(UINT8_C(0xdb),
                                   "MIPSNES2 STP STOP") == 0);
    CHECK(init_machine() == 0);
    CHECK(mipsnes2_machine_run_until(
              &machine, UINT64_C(5000), 100U, 1U, &sequences,
              &report, &timing_result) == MIPSNES2_MACHINE_RUN_CPU_STOP);
    CHECK(timing_result == MIPSNES2_CPU_TIMING_OK);
    CHECK(sequences == 3U);
    CHECK(report.cpu_result == MIPSNES2_CPU_CLOCK_STOPPED);
    CHECK(machine.cpu.clock_stopped && !machine.cpu.waiting);
    CHECK(machine.scheduler.now < UINT64_C(5000));
    return 0;
}

int main(void)
{
    CHECK(test_exact_nmi_wake() == 0);
    CHECK(test_wait_reaches_horizon() == 0);
    CHECK(test_masked_timer_irq_wake() == 0);
    CHECK(test_stp_remains_cpu_stop() == 0);
    (void)puts("machine WAI deterministic idle/wake integration: OK");
    return 0;
}
