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

typedef struct {
    mipsnes2_scheduler *scheduler;
    mipsnes2_hv_timing *hv_timing;
    mipsnes2_snes_bus *bus;
    size_t event_budget;
    mipsnes2_cpu_timing_report *report;
    mipsnes2_cpu_timing_result result;
} step_clock;

static bool advance_step_clock(void *opaque, uint64_t clocks)
{
    step_clock *clock = opaque;
    size_t dispatched = 0U;

    if (clock->result != MIPSNES2_CPU_TIMING_OK) {
        return false;
    }
    if (clock->scheduler->now > UINT64_MAX - clocks) {
        clock->result = MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
        return false;
    }
    clock->report->target_time = clock->scheduler->now + clocks;
    clock->result = run_until(
        clock->scheduler, clock->hv_timing, clock->bus->cpu_io_state,
        clock->report->target_time,
        clock->event_budget - clock->report->events_dispatched,
        &dispatched, clock->report);
    clock->report->events_dispatched += dispatched;
    return clock->result == MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result bus_fault_result(
    const mipsnes2_snes_bus *bus)
{
    if (bus->timing_overflow) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    if (bus->timing_fault == MIPSNES2_SNES_BUS_REENTRANT_ACCESS) {
        return MIPSNES2_CPU_TIMING_REENTRANT_ACCESS;
    }
    return bus->timing_fault == MIPSNES2_SNES_BUS_OK
               ? MIPSNES2_CPU_TIMING_OK
               : MIPSNES2_CPU_TIMING_RECOVERY_REQUIRED;
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
    uint64_t start_time;
    bool cpu_started = false;
    step_clock clock;
    mipsnes2_snes_bus_clock sink;
    mipsnes2_cpu_timing_result timing_result;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
        report->cpu_result = MIPSNES2_CPU_NOT_READY;
        report->scheduler_result = MIPSNES2_SCHEDULER_OK;
        report->hv_result = MIPSNES2_HV_OK;
    }
    if (cpu == NULL || bus == NULL || scheduler == NULL || report == NULL ||
        bus->wram == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (bus->clock != NULL || bus->transaction_active) {
        bus->timing_fault = MIPSNES2_SNES_BUS_REENTRANT_ACCESS;
        report->recovery_required = true;
        return MIPSNES2_CPU_TIMING_REENTRANT_ACCESS;
    }
    if (bus->timing_fault != MIPSNES2_SNES_BUS_OK || bus->timing_overflow) {
        report->recovery_required = true;
        return MIPSNES2_CPU_TIMING_RECOVERY_REQUIRED;
    }
    if (!mipsnes2_snes_bus_interface_matches(bus, cpu->bus)) {
        return MIPSNES2_CPU_TIMING_BUS_MISMATCH;
    }
    if (hv_timing != NULL && bus->cpu_io_state == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (bus->ppu_counters != NULL && bus->hv_timing != hv_timing) {
        return MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH;
    }
    if (interrupt_lines != NULL && hv_timing == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    cycles_before = cpu->instruction_cycles;
    accesses_before = bus->access_count;
    clocks_before = bus->master_clocks;
    start_time = scheduler->now;
    clock.scheduler = scheduler;
    clock.hv_timing = hv_timing;
    clock.bus = bus;
    clock.event_budget = event_budget;
    clock.report = report;
    clock.result = MIPSNES2_CPU_TIMING_OK;
    sink.context = &clock;
    sink.advance = advance_step_clock;
    bus->clock = &sink;

    /* Current-time events are outside the CPU but may not reenter its bus. */
    bus->transaction_active = true;
    (void)advance_step_clock(&clock, 0U);
    bus->transaction_active = false;
    timing_result = clock.result != MIPSNES2_CPU_TIMING_OK
                        ? clock.result : bus_fault_result(bus);
    if (timing_result != MIPSNES2_CPU_TIMING_OK) {
        goto finish;
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

    cpu_started = true;
    report->cpu_result = mipsnes2_cpu_step(cpu);

    if (cpu->instruction_cycles < cycles_before ||
        bus->access_count < accesses_before ||
        bus->master_clocks < clocks_before) {
        timing_result = MIPSNES2_CPU_TIMING_COUNTER_REGRESSION;
        goto finish;
    }
    report->processor_cycles = cpu->instruction_cycles - cycles_before;
    report->bus_accesses = bus->access_count - accesses_before;
    timing_result = clock.result != MIPSNES2_CPU_TIMING_OK
                        ? clock.result : bus_fault_result(bus);
    if (timing_result != MIPSNES2_CPU_TIMING_OK) {
        goto finish;
    }

    if (report->cpu_result == MIPSNES2_CPU_OK) {
        if (report->bus_accesses > report->processor_cycles) {
            timing_result = MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
            goto finish;
        }
        report->internal_cycles =
            report->processor_cycles - report->bus_accesses;
        if (mipsnes2_snes_bus_account_internal_cycles(
                bus, report->internal_cycles) != MIPSNES2_SNES_BUS_OK) {
            timing_result = clock.result != MIPSNES2_CPU_TIMING_OK
                                ? clock.result : bus_fault_result(bus);
            goto finish;
        }
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
finish:
    /* Never leave a pointer to this stack-owned sink in caller-owned state. */
    bus->clock = NULL;
    bus->transaction_active = false;
    if (timing_result != MIPSNES2_CPU_TIMING_OK && cpu_started &&
        bus->timing_fault == MIPSNES2_SNES_BUS_OK) {
        bus->timing_fault = MIPSNES2_SNES_BUS_CLOCK_ERROR;
    }
    report->recovery_required = bus->timing_fault != MIPSNES2_SNES_BUS_OK;
    if (bus->master_clocks >= clocks_before) {
        report->master_clocks = bus->master_clocks - clocks_before;
    }
    if (scheduler->now >= start_time) {
        report->elapsed_master_clocks = scheduler->now - start_time;
    }
    return timing_result;
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
