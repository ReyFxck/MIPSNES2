/* MIPSNES2 shared master-clock domain foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_MASTER_TIMING_H
#define MIPSNES2_MASTER_TIMING_H

#include <stddef.h>

#include "core/bus/cpu_io.h"
#include "core/scheduler/hv_timing.h"
#include "core/scheduler/scheduler.h"

typedef enum {
    MIPSNES2_MASTER_TIMING_OK = 0,
    MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT,
    MIPSNES2_MASTER_TIMING_DOMAIN_MISMATCH,
    MIPSNES2_MASTER_TIMING_TIME_REGRESSION,
    MIPSNES2_MASTER_TIMING_HV_ERROR,
    MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR
} mipsnes2_master_timing_result;

typedef struct {
    mipsnes2_scheduler_result scheduler_result;
    mipsnes2_hv_result hv_result;
    mipsnes2_master_clock requested_time;
    size_t events_dispatched;
} mipsnes2_master_timing_report;

mipsnes2_master_timing_result mipsnes2_master_timing_run_until(
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_cpu_io *cpu_io,
    mipsnes2_master_clock target,
    size_t event_budget,
    mipsnes2_master_timing_report *report);

#endif
