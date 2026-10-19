/* MIPSNES2 CPU/scheduler timing bridge. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/cpu_timing.h"

#include "core/scheduler/master_timing.h"

#include <limits.h>
#include <string.h>

static mipsnes2_cpu_timing_result run_until(
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_cpu_io *cpu_io,
    mipsnes2_master_clock target,
    size_t event_budget,
    size_t *events_dispatched,
    mipsnes2_cpu_timing_report *report)
{
    if (hv_timing != NULL) {
        mipsnes2_master_timing_report master_report;
        mipsnes2_master_timing_result result =
            mipsnes2_master_timing_run_until(
                scheduler, hv_timing, cpu_io, target, event_budget,
                &master_report);

        *events_dispatched = master_report.events_dispatched;
        report->scheduler_result = master_report.scheduler_result;
        report->hv_result = master_report.hv_result;
        if (result == MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT) {
            return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
        }
        if (result == MIPSNES2_MASTER_TIMING_DOMAIN_MISMATCH ||
            result == MIPSNES2_MASTER_TIMING_TIME_REGRESSION) {
            return MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH;
        }
        if (result == MIPSNES2_MASTER_TIMING_HV_ERROR) {
            return MIPSNES2_CPU_TIMING_HV_ERROR;
        }
        if (result != MIPSNES2_MASTER_TIMING_OK) {
            return MIPSNES2_CPU_TIMING_SCHEDULER_ERROR;
        }
        return MIPSNES2_CPU_TIMING_OK;
    }

    report->scheduler_result = mipsnes2_scheduler_run_until(
        scheduler, target, event_budget, events_dispatched);
    if (report->scheduler_result != MIPSNES2_SCHEDULER_OK) {
        return MIPSNES2_CPU_TIMING_SCHEDULER_ERROR;
    }
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result timed_step(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    uint64_t cycles_before;
    uint64_t accesses_before;
    uint64_t clocks_before;
    uint64_t clocks_after_accesses;
    size_t dispatched_before = 0U;
    size_t dispatched_after = 0U;
    mipsnes2_cpu_timing_result timing_result;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
        report->cpu_result = MIPSNES2_CPU_NOT_READY;
        report->scheduler_result = MIPSNES2_SCHEDULER_OK;
        report->hv_result = MIPSNES2_HV_OK;
    }
    if (cpu == NULL || bus == NULL || scheduler == NULL || report == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (!mipsnes2_snes_bus_interface_matches(bus, cpu->bus)) {
        return MIPSNES2_CPU_TIMING_BUS_MISMATCH;
    }
    if (hv_timing != NULL && bus->cpu_io_state == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (interrupt_lines != NULL && hv_timing == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (bus->timing_overflow) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }

    timing_result = run_until(scheduler, hv_timing, bus->cpu_io_state,
                              scheduler->now, event_budget,
                              &dispatched_before, report);
    report->events_dispatched = dispatched_before;
    if (timing_result != MIPSNES2_CPU_TIMING_OK) {
        return timing_result;
    }
    if (interrupt_lines != NULL) {
        bool delivered;

        (void)mipsnes2_interrupt_lines_sample_cpu_io(
            interrupt_lines, bus->cpu_io_state);
        (void)mipsnes2_interrupt_lines_drive_cpu(
            interrupt_lines, cpu, &delivered);
        report->nmi_delivered = delivered;
        report->irq_level_delivered = cpu->irq_level;
    }

    cycles_before = cpu->instruction_cycles;
    accesses_before = bus->access_count;
    clocks_before = bus->master_clocks;
    report->cpu_result = mipsnes2_cpu_step(cpu);

    if (cpu->instruction_cycles < cycles_before ||
        bus->access_count < accesses_before ||
        bus->master_clocks < clocks_before) {
        return MIPSNES2_CPU_TIMING_COUNTER_REGRESSION;
    }
    if (bus->timing_overflow) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }

    report->processor_cycles = cpu->instruction_cycles - cycles_before;
    report->bus_accesses = bus->access_count - accesses_before;
    clocks_after_accesses = bus->master_clocks;

    if (report->cpu_result == MIPSNES2_CPU_OK) {
        if (report->bus_accesses > report->processor_cycles) {
            return MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
        }
        report->internal_cycles =
            report->processor_cycles - report->bus_accesses;
        if (mipsnes2_snes_bus_account_internal_cycles(
                bus, report->internal_cycles) != MIPSNES2_SNES_BUS_OK) {
            return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
        }
    }

    if (bus->master_clocks < clocks_after_accesses ||
        bus->master_clocks < clocks_before) {
        return MIPSNES2_CPU_TIMING_COUNTER_REGRESSION;
    }
    report->master_clocks = bus->master_clocks - clocks_before;
    if (scheduler->now > UINT64_MAX - report->master_clocks) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    report->target_time = scheduler->now + report->master_clocks;

    timing_result = run_until(
        scheduler, hv_timing, bus->cpu_io_state, report->target_time,
        event_budget - dispatched_before, &dispatched_after, report);
    report->events_dispatched += dispatched_after;
    if (timing_result != MIPSNES2_CPU_TIMING_OK) {
        return timing_result;
    }
    if (interrupt_lines != NULL) {
        bool delivered;

        (void)mipsnes2_interrupt_lines_sample_cpu_io(
            interrupt_lines, bus->cpu_io_state);
        (void)mipsnes2_interrupt_lines_drive_cpu(
            interrupt_lines, cpu, &delivered);
        report->nmi_delivered = report->nmi_delivered || delivered;
        report->irq_level_delivered = cpu->irq_level;
    }
    return MIPSNES2_CPU_TIMING_OK;
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_step(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    return timed_step(cpu, bus, scheduler, NULL, NULL, event_budget, report);
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_step_hv(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    if (hv_timing == NULL) {
        if (report != NULL) {
            (void)memset(report, 0, sizeof(*report));
            report->cpu_result = MIPSNES2_CPU_NOT_READY;
            report->scheduler_result = MIPSNES2_SCHEDULER_OK;
            report->hv_result = MIPSNES2_HV_OK;
        }
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    return timed_step(cpu, bus, scheduler, hv_timing, NULL,
                      event_budget, report);
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_step_hv_interrupts(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    if (hv_timing == NULL || interrupt_lines == NULL) {
        if (report != NULL) {
            (void)memset(report, 0, sizeof(*report));
            report->cpu_result = MIPSNES2_CPU_NOT_READY;
            report->scheduler_result = MIPSNES2_SCHEDULER_OK;
            report->hv_result = MIPSNES2_HV_OK;
        }
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    return timed_step(cpu, bus, scheduler, hv_timing, interrupt_lines,
                      event_budget, report);
}
