/* MIPSNES2 CPU/scheduler timing bridge. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CPU_TIMING_H
#define MIPSNES2_CPU_TIMING_H

#include <stddef.h>
#include <stdint.h>

#include "core/bus/snes_bus.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/hv_timing.h"
#include "core/scheduler/scheduler.h"

typedef enum {
    MIPSNES2_CPU_TIMING_OK = 0,
    MIPSNES2_CPU_TIMING_INVALID_ARGUMENT,
    MIPSNES2_CPU_TIMING_BUS_MISMATCH,
    MIPSNES2_CPU_TIMING_COUNTER_REGRESSION,
    MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH,
    MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW,
    MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH,
    MIPSNES2_CPU_TIMING_HV_ERROR,
    MIPSNES2_CPU_TIMING_SCHEDULER_ERROR
} mipsnes2_cpu_timing_result;

typedef struct {
    mipsnes2_cpu_result cpu_result;
    mipsnes2_scheduler_result scheduler_result;
    mipsnes2_hv_result hv_result;
    uint64_t processor_cycles;
    uint64_t bus_accesses;
    uint64_t internal_cycles;
    uint64_t master_clocks;
    mipsnes2_master_clock target_time;
    size_t events_dispatched;
} mipsnes2_cpu_timing_report;

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_step(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report);
mipsnes2_cpu_timing_result mipsnes2_cpu_timed_step_hv(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report);

#endif
