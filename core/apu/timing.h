/* MIPSNES2 deterministic APU clock bridge. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_APU_TIMING_H
#define MIPSNES2_APU_TIMING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/apu/dsp/dsp.h"
#include "core/apu/spc700/cpu.h"
#include "core/scheduler/hv_timing.h"
#include "core/scheduler/scheduler.h"

enum {
    MIPSNES2_APU_SMP_HZ = 1024000,
    MIPSNES2_APU_NTSC_MASTER_HZ = 21477273,
    MIPSNES2_APU_PAL_MASTER_HZ = 21281370
};

typedef enum {
    MIPSNES2_APU_TIMING_OK = 0,
    MIPSNES2_APU_TIMING_INVALID_ARGUMENT,
    MIPSNES2_APU_TIMING_NOT_READY,
    MIPSNES2_APU_TIMING_TIME_REGRESSION,
    MIPSNES2_APU_TIMING_CLOCK_OVERFLOW,
    MIPSNES2_APU_TIMING_DEVICE_ERROR,
    MIPSNES2_APU_TIMING_CPU_ERROR
} mipsnes2_apu_timing_result;

typedef struct {
    mipsnes2_spc700_result cpu_result;
    uint64_t smp_cycles_generated;
    uint64_t instructions;
    uint64_t samples_generated;
    uint64_t pending_instruction_cycles;
    uint64_t target_master_clock;
} mipsnes2_apu_timing_report;

/* The rational ratio is a deterministic nominal-frequency policy. Real S-SMP
 * oscillator drift and sub-cycle S-CPU/S-SMP race order remain outside it. */
typedef struct {
    mipsnes2_clock_ratio master_to_smp;
    mipsnes2_spc700 *cpu;
    mipsnes2_smp_memory *memory;
    mipsnes2_dsp *dsp;
    uint64_t master_clocks;
    uint64_t smp_cycles;
    uint64_t instruction_credit;
    bool initialized;
} mipsnes2_apu_timing;

bool mipsnes2_apu_timing_init(mipsnes2_apu_timing *timing,
                              mipsnes2_hv_standard standard,
                              mipsnes2_spc700 *cpu,
                              mipsnes2_smp_memory *memory,
                              mipsnes2_dsp *dsp);

/* Advances to an absolute SNES master-clock time. Zero instruction_limit means
 * unlimited; a nonzero limit delays excess CPU work as cycle credit while
 * timers and the DSP cadence still reach target_master_clock. */
mipsnes2_apu_timing_result mipsnes2_apu_timing_run_until(
    mipsnes2_apu_timing *timing, uint64_t target_master_clock,
    size_t instruction_limit, mipsnes2_apu_timing_report *report);

#endif
