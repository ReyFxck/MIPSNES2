/* MIPSNES2 PS2 core diagnostic fixtures. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/diagnostic_suite.h"

#include <stdint.h>
#include <string.h>

#include "core/bus/snes_bus.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/cpu_timing.h"
#include "core/scheduler/scheduler.h"

typedef struct {
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state timing;
    mipsnes2_cpu_timing_report report;
    uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
    uint8_t rom[UINT32_C(0x10000)];
} diagnostic_fixture;

/* Keep target buffers out of the EE stack. */
static diagnostic_fixture fixture;

static bool fixture_rom_read(void *context, mipsnes2_address address,
                             uint8_t *value)
{
    diagnostic_fixture *f = context;

    if ((uint8_t)(address >> 16) != 0U ||
        (uint16_t)address < UINT16_C(0x8000)) {
        return false;
    }
    *value = f->rom[(uint16_t)address];
    return true;
}

static bool fixture_begin(void)
{
    mipsnes2_snes_bus_config config;

    (void)memset(&fixture, 0, sizeof(fixture));
    (void)memset(&config, 0, sizeof(config));
    fixture.rom[UINT16_C(0xfffc)] = UINT8_C(0x00);
    fixture.rom[UINT16_C(0xfffd)] = UINT8_C(0x80);
    config.wram = fixture.wram;
    config.wram_size = sizeof(fixture.wram);
    config.initial_open_value = UINT8_C(0xa5);
    config.cartridge.context = &fixture;
    config.cartridge.read8 = fixture_rom_read;
    if (mipsnes2_snes_bus_init(&fixture.bus, &config) !=
        MIPSNES2_SNES_BUS_OK) {
        return false;
    }
    mipsnes2_cpu_init(&fixture.cpu,
                     mipsnes2_snes_bus_make_interface(&fixture.bus));
    if (mipsnes2_cpu_reset(&fixture.cpu) != MIPSNES2_CPU_OK) {
        return false;
    }
    mipsnes2_scheduler_init(&fixture.scheduler);
    mipsnes2_cpu_timing_state_init(&fixture.timing);
    return true;
}

static bool test_cpu_lda_adc_sta(void)
{
    if (!fixture_begin()) {
        return false;
    }
    fixture.rom[UINT16_C(0x8000)] = UINT8_C(0xa9); /* LDA #$3c */
    fixture.rom[UINT16_C(0x8001)] = UINT8_C(0x3c);
    fixture.rom[UINT16_C(0x8002)] = UINT8_C(0x69); /* ADC #$04 */
    fixture.rom[UINT16_C(0x8003)] = UINT8_C(0x04);
    fixture.rom[UINT16_C(0x8004)] = UINT8_C(0x8d); /* STA $0002 */
    fixture.rom[UINT16_C(0x8005)] = UINT8_C(0x02);
    fixture.rom[UINT16_C(0x8006)] = UINT8_C(0x00);
    return mipsnes2_cpu_step(&fixture.cpu) == MIPSNES2_CPU_OK &&
           mipsnes2_cpu_step(&fixture.cpu) == MIPSNES2_CPU_OK &&
           mipsnes2_cpu_step(&fixture.cpu) == MIPSNES2_CPU_OK &&
           fixture.cpu.a == UINT16_C(0x0040) &&
           fixture.cpu.pc == UINT16_C(0x8007) &&
           fixture.wram[2] == UINT8_C(0x40);
}

static bool test_timed_nop_horizon(void)
{
    if (!fixture_begin()) {
        return false;
    }
    fixture.rom[UINT16_C(0x8000)] = UINT8_C(0xea); /* NOP */
    if (mipsnes2_cpu_timed_resume(
            &fixture.cpu, &fixture.bus, &fixture.scheduler, &fixture.timing,
            UINT64_C(8), 0U, &fixture.report) !=
        MIPSNES2_CPU_TIMING_HORIZON_REACHED ||
        fixture.scheduler.now != UINT64_C(8) ||
        fixture.cpu.pc != UINT16_C(0x8001)) {
        return false;
    }
    return mipsnes2_cpu_timed_resume(
               &fixture.cpu, &fixture.bus, &fixture.scheduler,
               &fixture.timing, UINT64_C(14), 0U, &fixture.report) ==
               MIPSNES2_CPU_TIMING_OK &&
           fixture.scheduler.now == UINT64_C(14) &&
           fixture.report.processor_cycles == UINT64_C(2) &&
           fixture.report.bus_accesses == UINT64_C(1) &&
           fixture.report.internal_cycles == UINT64_C(1) &&
           fixture.cpu.pc == UINT16_C(0x8001);
}

static bool test_abort_port_write(bool external)
{
    mipsnes2_cpu_abort_write_disposition disposition = external
        ? MIPSNES2_CPU_ABORT_WRITE_EXTERNAL
        : MIPSNES2_CPU_ABORT_WRITE_INHIBIT;

    if (!fixture_begin()) {
        return false;
    }
    fixture.rom[UINT16_C(0x8000)] = UINT8_C(0x8d); /* STA $2180 */
    fixture.rom[UINT16_C(0x8001)] = UINT8_C(0x80);
    fixture.rom[UINT16_C(0x8002)] = UINT8_C(0x21);
    fixture.cpu.a = UINT16_C(0x005a);
    if (mipsnes2_cpu_timed_resume(
            &fixture.cpu, &fixture.bus, &fixture.scheduler, &fixture.timing,
            UINT64_C(27), 0U, &fixture.report) !=
        MIPSNES2_CPU_TIMING_HORIZON_REACHED ||
        mipsnes2_cpu_timing_pulse_abort(&fixture.timing) !=
            MIPSNES2_CPU_TIMING_OK ||
        mipsnes2_cpu_timed_resume(
            &fixture.cpu, &fixture.bus, &fixture.scheduler, &fixture.timing,
            UINT64_MAX, 0U, &fixture.report) !=
            MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED ||
        fixture.scheduler.now != UINT64_C(30) ||
        fixture.wram[0] != 0U ||
        mipsnes2_cpu_timing_resolve_abort_write(&fixture.timing,
                                                disposition) !=
            MIPSNES2_CPU_TIMING_OK) {
        return false;
    }
    return mipsnes2_cpu_timed_resume(
               &fixture.cpu, &fixture.bus, &fixture.scheduler,
               &fixture.timing, UINT64_MAX, 0U, &fixture.report) ==
               MIPSNES2_CPU_TIMING_OK &&
           fixture.scheduler.now == UINT64_C(30) &&
           fixture.report.bus_accesses == UINT64_C(4) &&
           fixture.cpu.abort_pending &&
           fixture.cpu.pc == UINT16_C(0x8000) &&
           fixture.wram[0] == (external ? UINT8_C(0x5a) : 0U) &&
           fixture.bus.wram_port_address == (external ? 1U : 0U) &&
           fixture.bus.open_value == UINT8_C(0x5a);
}

static bool test_abort_late_rmw_status(void)
{
    if (!fixture_begin()) {
        return false;
    }
    fixture.rom[UINT16_C(0x8000)] = UINT8_C(0xe6); /* INC $00 */
    fixture.rom[UINT16_C(0x8001)] = 0U;
    fixture.wram[0] = UINT8_C(0x7f);
    fixture.cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_N;
    if (mipsnes2_cpu_timed_resume(
            &fixture.cpu, &fixture.bus, &fixture.scheduler, &fixture.timing,
            UINT64_C(30), 0U, &fixture.report) !=
        MIPSNES2_CPU_TIMING_HORIZON_REACHED ||
        !fixture.timing.cpu_cycle.rmw_modify_committed ||
        (fixture.cpu.p & MIPSNES2_CPU_FLAG_N) == 0U ||
        mipsnes2_cpu_timing_pulse_abort(&fixture.timing) !=
            MIPSNES2_CPU_TIMING_OK ||
        mipsnes2_cpu_timed_resume(
            &fixture.cpu, &fixture.bus, &fixture.scheduler, &fixture.timing,
            UINT64_MAX, 0U, &fixture.report) !=
            MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED ||
        mipsnes2_cpu_timing_resolve_abort_write(
            &fixture.timing, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) !=
            MIPSNES2_CPU_TIMING_OK) {
        return false;
    }
    return mipsnes2_cpu_timed_resume(
               &fixture.cpu, &fixture.bus, &fixture.scheduler,
               &fixture.timing, UINT64_MAX, 0U, &fixture.report) ==
               MIPSNES2_CPU_TIMING_OK &&
           fixture.cpu.abort_pending &&
           fixture.cpu.pc == UINT16_C(0x8000) &&
           (fixture.cpu.p & MIPSNES2_CPU_FLAG_N) != 0U &&
           fixture.wram[0] == UINT8_C(0x7f);
}

void mipsnes2_ps2_diagnostic_run(mipsnes2_ps2_diagnostic_report *report,
                                 mipsnes2_ps2_diagnostic_emit emit,
                                 void *context)
{
    static const char *const names[] = {
        "CPU LDA ADC STA", "TIMED NOP HORIZON", "ABORT EXTERNAL WRITE",
        "ABORT INHIBITED WRITE", "ABORT LATE RMW STATUS"
    };
    bool results[5];
    unsigned i;

    if (report == NULL) {
        return;
    }
    report->passed = 0U;
    report->failed = 0U;
    results[0] = test_cpu_lda_adc_sta();
    results[1] = test_timed_nop_horizon();
    results[2] = test_abort_port_write(true);
    results[3] = test_abort_port_write(false);
    results[4] = test_abort_late_rmw_status();
    for (i = 0U; i < 5U; ++i) {
        if (results[i]) {
            report->passed++;
        } else {
            report->failed++;
        }
        if (emit != NULL) {
            emit(context, names[i], results[i]);
        }
    }
}
