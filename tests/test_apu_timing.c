/* Deterministic master-clock to S-SMP timing fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/timing.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

typedef struct {
    uint8_t ram[MIPSNES2_SMP_RAM_SIZE];
    mipsnes2_apu_io_ports ports;
    mipsnes2_dsp dsp;
    mipsnes2_smp_memory memory;
    mipsnes2_spc700 cpu;
    mipsnes2_apu_timing timing;
} fixture;

static bool fixture_init(fixture *item, const uint8_t *program,
                         size_t program_size, bool with_cpu)
{
    mipsnes2_spc700_registers registers = {0};

    (void)memset(item, 0, sizeof(*item));
    (void)memcpy(item->ram + UINT16_C(0x2000), program, program_size);
    mipsnes2_apu_io_ports_init(&item->ports);
    mipsnes2_dsp_init(&item->dsp);
    if (!mipsnes2_smp_memory_init(&item->memory, item->ram,
                                  sizeof(item->ram), &item->ports,
                                  NULL, 0U) ||
        !mipsnes2_smp_memory_attach_dsp(&item->memory, &item->dsp)) {
        return false;
    }
    registers.pc = UINT16_C(0x2000);
    if (with_cpu && !mipsnes2_spc700_init(
            &item->cpu, &item->memory, &registers)) {
        return false;
    }
    return mipsnes2_apu_timing_init(
        &item->timing, MIPSNES2_HV_STANDARD_NTSC,
        with_cpu ? &item->cpu : NULL, &item->memory, &item->dsp);
}

int main(void)
{
    static const uint8_t nops[64] = {0};
    static const uint8_t sleep_program[] = {UINT8_C(0xef)};
    fixture direct;
    fixture chunked;
    fixture limited;
    fixture sleeping;
    mipsnes2_apu_timing_report report;
    uint64_t target;

    CHECK(fixture_init(&direct, nops, sizeof(nops), false));
    CHECK(mipsnes2_apu_timing_run_until(
              &direct.timing, MIPSNES2_APU_NTSC_MASTER_HZ, 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(report.smp_cycles_generated == MIPSNES2_APU_SMP_HZ);
    CHECK(direct.timing.smp_cycles == MIPSNES2_APU_SMP_HZ);
    CHECK(direct.dsp.sample_count == UINT64_C(32000));
    CHECK(report.samples_generated == UINT64_C(32000));

    CHECK(fixture_init(&chunked, nops, sizeof(nops), false));
    target = MIPSNES2_APU_NTSC_MASTER_HZ / 3U;
    CHECK(mipsnes2_apu_timing_run_until(
              &chunked.timing, target, 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    target = (MIPSNES2_APU_NTSC_MASTER_HZ / 3U) * 2U;
    CHECK(mipsnes2_apu_timing_run_until(
              &chunked.timing, target, 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(mipsnes2_apu_timing_run_until(
              &chunked.timing, MIPSNES2_APU_NTSC_MASTER_HZ, 0U,
              &report) == MIPSNES2_APU_TIMING_OK);
    CHECK(chunked.timing.smp_cycles == direct.timing.smp_cycles);
    CHECK(chunked.dsp.sample_count == direct.dsp.sample_count);
    CHECK(chunked.dsp.sample_phase == direct.dsp.sample_phase);

    CHECK(fixture_init(&direct, nops, sizeof(nops), true));
    CHECK(mipsnes2_smp_memory_write(
        &direct.memory, UINT16_C(0x00fc), UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(
        &direct.memory, UINT16_C(0x00f1), UINT8_C(0x04)));
    CHECK(mipsnes2_apu_timing_run_until(
              &direct.timing, UINT64_C(1000), 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(direct.timing.smp_cycles == UINT64_C(47));
    CHECK(direct.cpu.instruction_cycles == UINT64_C(46));
    CHECK(direct.cpu.registers.pc == UINT16_C(0x2017));
    CHECK(direct.timing.instruction_credit == 1U);
    CHECK(direct.dsp.sample_count == 1U && direct.dsp.sample_phase == 15U);

    CHECK(fixture_init(&chunked, nops, sizeof(nops), true));
    CHECK(mipsnes2_smp_memory_write(
        &chunked.memory, UINT16_C(0x00fc), UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(
        &chunked.memory, UINT16_C(0x00f1), UINT8_C(0x04)));
    for (target = 100U; target <= 1000U; target += 100U) {
        CHECK(mipsnes2_apu_timing_run_until(
                  &chunked.timing, target, 0U, &report) ==
              MIPSNES2_APU_TIMING_OK);
    }
    CHECK(chunked.cpu.registers.pc == direct.cpu.registers.pc);
    CHECK(chunked.cpu.instruction_cycles == direct.cpu.instruction_cycles);
    CHECK(chunked.timing.instruction_credit ==
          direct.timing.instruction_credit);
    CHECK(chunked.memory.timers.stage1_phase ==
          direct.memory.timers.stage1_phase);
    CHECK(chunked.memory.timers.output[2] ==
          direct.memory.timers.output[2]);
    CHECK(chunked.dsp.sample_count == direct.dsp.sample_count &&
          chunked.dsp.sample_phase == direct.dsp.sample_phase);

    CHECK(fixture_init(&limited, nops, sizeof(nops), true));
    CHECK(mipsnes2_apu_timing_run_until(
              &limited.timing, UINT64_C(1000), 1U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(report.instructions == 1U && limited.cpu.registers.pc ==
          UINT16_C(0x2001));
    CHECK(limited.timing.instruction_credit == UINT64_C(45));
    CHECK(mipsnes2_apu_timing_run_until(
              &limited.timing, UINT64_C(1000), 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(limited.cpu.registers.pc == direct.cpu.registers.pc);
    CHECK(limited.timing.instruction_credit == 1U);

    CHECK(fixture_init(&sleeping, sleep_program,
                       sizeof(sleep_program), true));
    CHECK(mipsnes2_apu_timing_run_until(
              &sleeping.timing, UINT64_C(100), 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(sleeping.cpu.wait_state == MIPSNES2_SPC700_SLEEPING);
    CHECK(sleeping.cpu.instruction_cycles == 3U);
    CHECK(mipsnes2_apu_timing_run_until(
              &sleeping.timing, UINT64_C(1000), 0U, &report) ==
          MIPSNES2_APU_TIMING_OK);
    CHECK(report.cpu_result == MIPSNES2_SPC700_HALTED);
    CHECK(sleeping.cpu.instruction_cycles == 3U &&
          sleeping.dsp.sample_count == 1U);

    CHECK(mipsnes2_apu_timing_run_until(
              &sleeping.timing, UINT64_C(999), 0U, &report) ==
          MIPSNES2_APU_TIMING_TIME_REGRESSION);
    sleeping.timing.smp_cycles = UINT64_MAX;
    CHECK(mipsnes2_apu_timing_run_until(
              &sleeping.timing, UINT64_C(2000), 0U, &report) ==
          MIPSNES2_APU_TIMING_CLOCK_OVERFLOW);
    CHECK(!mipsnes2_apu_timing_init(NULL, MIPSNES2_HV_STANDARD_NTSC,
                                    NULL, NULL, NULL));
    CHECK(mipsnes2_apu_timing_run_until(NULL, 0U, 0U, &report) ==
          MIPSNES2_APU_TIMING_INVALID_ARGUMENT);

    (void)puts("APU clock bridge: OK");
    return 0;
}
