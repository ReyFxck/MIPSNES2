/* MIPSNES2 deterministic APU clock bridge. See LICENSE for GPL-3.0 terms. */
#include "core/apu/timing.h"

#include <string.h>

static bool advance_devices(mipsnes2_apu_timing *timing, uint64_t cycles)
{
    mipsnes2_dsp next_dsp;

    if (cycles == 0U) {
        return true;
    }
    next_dsp = *timing->dsp;
    if (!mipsnes2_dsp_advance(&next_dsp, cycles) ||
        !mipsnes2_smp_memory_advance(timing->memory, cycles)) {
        return false;
    }
    *timing->dsp = next_dsp;
    return true;
}

static mipsnes2_apu_timing_result finish_cpu_error(
    mipsnes2_apu_timing *timing, uint64_t remaining,
    mipsnes2_spc700_result cpu_result, mipsnes2_apu_timing_report *report)
{
    if (!advance_devices(timing, remaining)) {
        return MIPSNES2_APU_TIMING_DEVICE_ERROR;
    }
    timing->instruction_credit += remaining;
    report->cpu_result = cpu_result;
    report->pending_instruction_cycles = timing->instruction_credit;
    report->samples_generated = timing->dsp->sample_count -
                                report->samples_generated;
    return MIPSNES2_APU_TIMING_CPU_ERROR;
}

bool mipsnes2_apu_timing_init(mipsnes2_apu_timing *timing,
                              mipsnes2_hv_standard standard,
                              mipsnes2_spc700 *cpu,
                              mipsnes2_smp_memory *memory,
                              mipsnes2_dsp *dsp)
{
    uint32_t master_hz;

    if (timing == NULL) {
        return false;
    }
    *timing = (mipsnes2_apu_timing){0};
    if ((standard != MIPSNES2_HV_STANDARD_NTSC &&
         standard != MIPSNES2_HV_STANDARD_PAL) ||
        memory == NULL || !memory->initialized || dsp == NULL ||
        !dsp->initialized || (cpu != NULL && !cpu->initialized)) {
        return false;
    }
    master_hz = standard == MIPSNES2_HV_STANDARD_NTSC
        ? MIPSNES2_APU_NTSC_MASTER_HZ : MIPSNES2_APU_PAL_MASTER_HZ;
    if (mipsnes2_clock_ratio_init(&timing->master_to_smp,
                                  MIPSNES2_APU_SMP_HZ, master_hz) !=
        MIPSNES2_SCHEDULER_OK) {
        return false;
    }
    timing->cpu = cpu;
    timing->memory = memory;
    timing->dsp = dsp;
    timing->initialized = true;
    return true;
}

mipsnes2_apu_timing_result mipsnes2_apu_timing_run_until(
    mipsnes2_apu_timing *timing, uint64_t target_master_clock,
    size_t instruction_limit, mipsnes2_apu_timing_report *report)
{
    mipsnes2_clock_ratio next_ratio;
    mipsnes2_dsp checked_dsp;
    uint64_t generated;
    uint64_t available;
    uint64_t samples_before;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
        report->cpu_result = MIPSNES2_SPC700_OK;
        report->target_master_clock = target_master_clock;
    }
    if (timing == NULL || report == NULL) {
        return MIPSNES2_APU_TIMING_INVALID_ARGUMENT;
    }
    if (!timing->initialized || timing->memory == NULL ||
        !timing->memory->initialized || timing->dsp == NULL ||
        !timing->dsp->initialized ||
        (timing->cpu != NULL && !timing->cpu->initialized)) {
        return MIPSNES2_APU_TIMING_NOT_READY;
    }
    if (target_master_clock < timing->master_clocks) {
        return MIPSNES2_APU_TIMING_TIME_REGRESSION;
    }
    next_ratio = timing->master_to_smp;
    if (mipsnes2_clock_ratio_advance(
            &next_ratio, target_master_clock - timing->master_clocks,
            &generated) != MIPSNES2_SCHEDULER_OK ||
        timing->smp_cycles > UINT64_MAX - generated ||
        timing->instruction_credit > UINT64_MAX - generated) {
        return MIPSNES2_APU_TIMING_CLOCK_OVERFLOW;
    }
    checked_dsp = *timing->dsp;
    if (!mipsnes2_dsp_advance(&checked_dsp, generated)) {
        return MIPSNES2_APU_TIMING_CLOCK_OVERFLOW;
    }

    samples_before = timing->dsp->sample_count;
    report->samples_generated = samples_before;
    report->smp_cycles_generated = generated;
    timing->master_to_smp = next_ratio;
    timing->master_clocks = target_master_clock;
    timing->smp_cycles += generated;
    available = generated;

    if (timing->cpu == NULL ||
        timing->cpu->wait_state != MIPSNES2_SPC700_RUNNING) {
        if (!advance_devices(timing, available)) {
            return MIPSNES2_APU_TIMING_DEVICE_ERROR;
        }
        timing->instruction_credit = 0U;
        report->cpu_result = timing->cpu == NULL
            ? MIPSNES2_SPC700_OK : MIPSNES2_SPC700_HALTED;
        report->samples_generated = timing->dsp->sample_count - samples_before;
        return MIPSNES2_APU_TIMING_OK;
    }

    for (;;) {
        uint8_t maximum_cycles;
        uint64_t need;
        uint64_t cycles_before;
        uint64_t actual_cycles;
        mipsnes2_spc700_result cpu_result;

        if (available == 0U && timing->instruction_credit < 2U) {
            break;
        }
        if (instruction_limit != 0U &&
            report->instructions >= instruction_limit) {
            if (!advance_devices(timing, available)) {
                return MIPSNES2_APU_TIMING_DEVICE_ERROR;
            }
            timing->instruction_credit += available;
            break;
        }
        cpu_result = mipsnes2_spc700_prepare_step(timing->cpu,
                                                  &maximum_cycles);
        if (cpu_result != MIPSNES2_SPC700_OK) {
            return finish_cpu_error(timing, available, cpu_result, report);
        }
        need = timing->instruction_credit >= maximum_cycles
            ? 0U : maximum_cycles - timing->instruction_credit;
        if (available < need) {
            if (!advance_devices(timing, available)) {
                return MIPSNES2_APU_TIMING_DEVICE_ERROR;
            }
            timing->instruction_credit += available;
            break;
        }
        if (!advance_devices(timing, need)) {
            return MIPSNES2_APU_TIMING_DEVICE_ERROR;
        }
        timing->instruction_credit += need;
        available -= need;
        cycles_before = timing->cpu->instruction_cycles;
        cpu_result = mipsnes2_spc700_step_clocked(timing->cpu);
        if (cpu_result != MIPSNES2_SPC700_OK) {
            return finish_cpu_error(timing, available, cpu_result, report);
        }
        actual_cycles = timing->cpu->instruction_cycles - cycles_before;
        if (actual_cycles > timing->instruction_credit) {
            report->cpu_result = MIPSNES2_SPC700_CYCLE_OVERFLOW;
            return MIPSNES2_APU_TIMING_CPU_ERROR;
        }
        timing->instruction_credit -= actual_cycles;
        ++report->instructions;
        if (timing->cpu->wait_state != MIPSNES2_SPC700_RUNNING) {
            if (!advance_devices(timing, available)) {
                return MIPSNES2_APU_TIMING_DEVICE_ERROR;
            }
            timing->instruction_credit = 0U;
            available = 0U;
            break;
        }
        if (available == 0U && timing->instruction_credit < 2U) {
            break;
        }
    }

    report->pending_instruction_cycles = timing->instruction_credit;
    report->samples_generated = timing->dsp->sample_count - samples_before;
    return MIPSNES2_APU_TIMING_OK;
}
