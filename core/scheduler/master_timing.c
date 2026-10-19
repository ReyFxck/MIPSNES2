/* MIPSNES2 shared master-clock domain foundation. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/master_timing.h"

#include <string.h>

static mipsnes2_master_timing_result hv_failure(
    mipsnes2_master_timing_report *report,
    mipsnes2_hv_result result)
{
    report->hv_result = result;
    return MIPSNES2_MASTER_TIMING_HV_ERROR;
}

static mipsnes2_master_timing_result scheduler_failure(
    mipsnes2_master_timing_report *report,
    mipsnes2_scheduler_result result)
{
    report->scheduler_result = result;
    return MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR;
}

static mipsnes2_master_timing_result advance_hv_to(
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_cpu_io *cpu_io,
    mipsnes2_master_clock target,
    mipsnes2_master_timing_report *report)
{
    mipsnes2_hv_timing next_timing = *hv_timing;
    mipsnes2_cpu_io next_cpu_io = *cpu_io;
    mipsnes2_hv_result result = mipsnes2_hv_timing_advance(
        &next_timing, &next_cpu_io,
        target - hv_timing->elapsed_master_clocks);

    if (result != MIPSNES2_HV_OK) {
        return hv_failure(report, result);
    }
    *hv_timing = next_timing;
    *cpu_io = next_cpu_io;
    return MIPSNES2_MASTER_TIMING_OK;
}

mipsnes2_master_timing_result mipsnes2_master_timing_run_until(
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_cpu_io *cpu_io,
    mipsnes2_master_clock target,
    size_t event_budget,
    mipsnes2_master_timing_report *report)
{
    size_t remaining_budget = event_budget;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
        report->scheduler_result = MIPSNES2_SCHEDULER_OK;
        report->hv_result = MIPSNES2_HV_OK;
        report->requested_time = target;
    }
    if (scheduler == NULL || hv_timing == NULL || cpu_io == NULL ||
        report == NULL) {
        return MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT;
    }
    if (scheduler->now != hv_timing->elapsed_master_clocks) {
        return MIPSNES2_MASTER_TIMING_DOMAIN_MISMATCH;
    }
    if (target < scheduler->now) {
        return MIPSNES2_MASTER_TIMING_TIME_REGRESSION;
    }
    if (scheduler->dispatching) {
        return scheduler_failure(report,
                                 MIPSNES2_SCHEDULER_REENTRANT_RUN);
    }
    if (mipsnes2_hv_timing_advance(hv_timing, cpu_io, 0U) !=
        MIPSNES2_HV_OK) {
        return hv_failure(report, MIPSNES2_HV_INVALID_ARGUMENT);
    }

    for (;;) {
        mipsnes2_master_clock next_event;
        mipsnes2_master_clock horizon = target;
        size_t dispatched = 0U;
        mipsnes2_scheduler_result scheduler_result;
        mipsnes2_master_timing_result timing_result;

        if (mipsnes2_scheduler_next_event_time(scheduler, &next_event) &&
            next_event <= target) {
            if (next_event < scheduler->now) {
                return scheduler_failure(
                    report, MIPSNES2_SCHEDULER_TIME_REGRESSION);
            }
            horizon = next_event;
            if (remaining_budget == 0U) {
                return scheduler_failure(
                    report, MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
            }
        }

        timing_result = advance_hv_to(hv_timing, cpu_io, horizon, report);
        if (timing_result != MIPSNES2_MASTER_TIMING_OK) {
            return timing_result;
        }
        scheduler_result = mipsnes2_scheduler_run_until(
            scheduler, horizon, remaining_budget, &dispatched);
        report->events_dispatched += dispatched;
        remaining_budget -= dispatched;
        if (scheduler_result != MIPSNES2_SCHEDULER_OK) {
            return scheduler_failure(report, scheduler_result);
        }
        if (horizon == target) {
            return MIPSNES2_MASTER_TIMING_OK;
        }
    }
}
