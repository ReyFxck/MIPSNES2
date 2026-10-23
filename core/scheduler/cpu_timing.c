/* MIPSNES2 CPU/scheduler timing bridge. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/cpu_timing.h"

#include "core/scheduler/5a22_timing.h"
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

static void initialize_report(mipsnes2_cpu_timing_report *report)
{
    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
        report->cpu_result = MIPSNES2_CPU_NOT_READY;
        report->scheduler_result = MIPSNES2_SCHEDULER_OK;
        report->hv_result = MIPSNES2_HV_OK;
    }
}

static bool operation_matches(const mipsnes2_cpu_operation *left,
                              const mipsnes2_cpu_operation *right)
{
    return left->kind == right->kind && left->address == right->address &&
           left->value == right->value;
}

static void fill_resumable_report(
    const mipsnes2_cpu_timing_state *state,
    const mipsnes2_cpu *cpu,
    const mipsnes2_snes_bus *bus,
    const mipsnes2_scheduler *scheduler,
    mipsnes2_cpu_timing_report *report)
{
    report->cpu_result = state->cpu_result;
    report->events_dispatched = state->events_dispatched;
    report->nmi_delivered = state->nmi_delivered;
    report->irq_level_delivered = state->irq_level_delivered;
    report->target_time = scheduler->now;

    if (!state->reset_sequence &&
        cpu->instruction_cycles >= state->processor_cycles_before) {
        report->processor_cycles =
            cpu->instruction_cycles - state->processor_cycles_before;
    }
    if (bus->access_count >= state->bus_accesses_before) {
        report->bus_accesses =
            bus->access_count - state->bus_accesses_before;
    }
    if (bus->master_clocks >= state->bus_clocks_before) {
        report->master_clocks =
            bus->master_clocks - state->bus_clocks_before;
    }
    if (!state->reset_sequence &&
        report->processor_cycles >= report->bus_accesses) {
        report->internal_cycles =
            report->processor_cycles - report->bus_accesses;
    }
}

static void sample_interrupt_lines(
    mipsnes2_cpu_timing_state *state,
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_interrupt_lines *interrupt_lines,
    bool drive_cpu)
{
    bool delivered = false;

    if (interrupt_lines == NULL || state->reset_sequence) {
        return;
    }
    (void)mipsnes2_interrupt_lines_sample_cpu_io(
        interrupt_lines, bus->cpu_io_state);
    if (drive_cpu) {
        (void)mipsnes2_interrupt_lines_drive_cpu(
            interrupt_lines, cpu, &delivered);
        state->nmi_delivered = state->nmi_delivered || delivered;
        state->irq_level_delivered = cpu->irq_level;
    }
}

static mipsnes2_cpu_timing_result add_dispatched_events(
    mipsnes2_cpu_timing_state *state,
    size_t dispatched)
{
    if (state->events_dispatched > SIZE_MAX - dispatched) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    state->events_dispatched += dispatched;
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result run_resumable_until(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock target,
    size_t *remaining_budget,
    mipsnes2_cpu_timing_report *report)
{
    size_t dispatched = 0U;
    mipsnes2_cpu_timing_result result = run_until(
        scheduler, hv_timing, bus->cpu_io_state, target,
        *remaining_budget, &dispatched, report);
    mipsnes2_cpu_timing_result add_result =
        add_dispatched_events(state, dispatched);

    if (dispatched <= *remaining_budget) {
        *remaining_budget -= dispatched;
    } else {
        *remaining_budget = 0U;
    }
    state->domain_time = scheduler->now;
    sample_interrupt_lines(state, cpu, bus, interrupt_lines, false);
    if (add_result != MIPSNES2_CPU_TIMING_OK) {
        return add_result;
    }
    return result;
}

static bool resumable_context_matches(
    const mipsnes2_cpu_timing_state *state,
    const mipsnes2_cpu *cpu,
    const mipsnes2_snes_bus *bus,
    const mipsnes2_scheduler *scheduler,
    const mipsnes2_hv_timing *hv_timing,
    const mipsnes2_interrupt_lines *interrupt_lines)
{
    return state->bound_cpu == cpu && state->bound_bus == bus &&
           state->bound_scheduler == scheduler &&
           state->bound_hv_timing == hv_timing &&
           state->bound_interrupt_lines == interrupt_lines;
}

static mipsnes2_cpu_timing_result validate_resumable_context(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    mipsnes2_cpu_timing_report *report)
{
    if (cpu == NULL || bus == NULL || scheduler == NULL || state == NULL ||
        report == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
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
    if (hv_timing != NULL &&
        scheduler->now != hv_timing->elapsed_master_clocks) {
        return MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH;
    }
    if (requested_horizon < scheduler->now) {
        return MIPSNES2_CPU_TIMING_TIME_REGRESSION;
    }
    if (bus->timing_overflow) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    if (state->active && !resumable_context_matches(
                             state, cpu, bus, scheduler, hv_timing,
                             interrupt_lines)) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    if (state->active && state->domain_time != scheduler->now) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    if (state->operation_pending &&
        (!state->active || !state->cpu_cycle.active)) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    if (!state->active &&
        (state->operation_pending || state->cpu_cycle.active)) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result operation_clock_cost(
    const mipsnes2_snes_bus *bus,
    const mipsnes2_cpu_operation *operation,
    uint8_t *master_clocks)
{
    switch (operation->kind) {
    case MIPSNES2_CPU_OPERATION_CONTROL:
        *master_clocks = 0U;
        return MIPSNES2_CPU_TIMING_OK;
    case MIPSNES2_CPU_OPERATION_READ:
    case MIPSNES2_CPU_OPERATION_WRITE:
        *master_clocks = mipsnes2_5a22_access_master_clocks(
            operation->address, bus->fastrom_enabled);
        return MIPSNES2_CPU_TIMING_OK;
    case MIPSNES2_CPU_OPERATION_INTERNAL:
        *master_clocks = MIPSNES2_5A22_INTERNAL_MASTER_CLOCKS;
        return MIPSNES2_CPU_TIMING_OK;
    case MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC:
    default:
        return MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION;
    }
}

static mipsnes2_cpu_timing_result prepare_pending_operation(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_cpu_timing_state *state,
    const mipsnes2_cpu_operation *operation)
{
    uint8_t clocks;
    mipsnes2_cpu_timing_result result =
        operation_clock_cost(bus, operation, &clocks);

    if (result != MIPSNES2_CPU_TIMING_OK) {
        return result;
    }
    if (operation->kind == MIPSNES2_CPU_OPERATION_CONTROL) {
        return MIPSNES2_CPU_TIMING_OK;
    }
    if (!state->reset_sequence && cpu->instruction_cycles == UINT64_MAX) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    if ((operation->kind == MIPSNES2_CPU_OPERATION_READ ||
         operation->kind == MIPSNES2_CPU_OPERATION_WRITE) &&
        bus->access_count == UINT64_MAX) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    if (bus->master_clocks > UINT64_MAX - clocks ||
        scheduler->now > UINT64_MAX - clocks) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }

    state->pending_operation = *operation;
    (void)memcpy(&state->pending_cpu_snapshot, cpu, sizeof(*cpu));
    state->pending_processor_cycles = cpu->instruction_cycles;
    state->pending_bus_accesses = bus->access_count;
    state->pending_bus_clocks = bus->master_clocks;
    state->operation_start_time = scheduler->now;
    state->operation_end_time = scheduler->now + clocks;
    state->operation_master_clocks = clocks;
    state->pending_fastrom_enabled = bus->fastrom_enabled;
    state->operation_pending = true;
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result validate_pending_operation(
    const mipsnes2_cpu *cpu,
    const mipsnes2_snes_bus *bus,
    const mipsnes2_scheduler *scheduler,
    const mipsnes2_cpu_timing_state *state)
{
    mipsnes2_cpu_operation current;

    if (scheduler->now > state->operation_end_time ||
        memcmp(cpu, &state->pending_cpu_snapshot, sizeof(*cpu)) != 0 ||
        cpu->instruction_cycles != state->pending_processor_cycles ||
        bus->access_count != state->pending_bus_accesses ||
        bus->master_clocks != state->pending_bus_clocks ||
        bus->fastrom_enabled != state->pending_fastrom_enabled) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    if (mipsnes2_cpu_cycle_peek(cpu, &state->cpu_cycle, &current) !=
            MIPSNES2_CPU_CYCLE_READY ||
        !operation_matches(&current, &state->pending_operation)) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result commit_control_operation(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_cpu_timing_state *state,
    const mipsnes2_cpu_operation *operation,
    mipsnes2_cpu_cycle_result *cycle_result)
{
    uint64_t cycles_before = cpu->instruction_cycles;
    uint64_t accesses_before = bus->access_count;
    uint64_t clocks_before = bus->master_clocks;

    *cycle_result = mipsnes2_cpu_cycle_commit(
        cpu, &state->cpu_cycle, operation, &state->cpu_result);
    if (*cycle_result != MIPSNES2_CPU_CYCLE_READY &&
        *cycle_result != MIPSNES2_CPU_CYCLE_COMPLETE) {
        return *cycle_result == MIPSNES2_CPU_CYCLE_STALE_OPERATION
                   ? MIPSNES2_CPU_TIMING_STALE_STATE
                   : MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (cpu->instruction_cycles != cycles_before ||
        bus->access_count != accesses_before ||
        bus->master_clocks != clocks_before) {
        return MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
    }
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result commit_pending_operation(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_cpu_cycle_result *cycle_result)
{
    uint64_t cycles_before = cpu->instruction_cycles;
    uint64_t accesses_before = bus->access_count;
    uint64_t clocks_before = bus->master_clocks;
    uint64_t expected_cycles = state->reset_sequence ? 0U : UINT64_C(1);
    uint64_t expected_accesses = 0U;
    mipsnes2_cpu_timing_result result;

    result = validate_pending_operation(
        cpu, bus, state->bound_scheduler, state);
    if (result != MIPSNES2_CPU_TIMING_OK) {
        return result;
    }
    *cycle_result = mipsnes2_cpu_cycle_commit(
        cpu, &state->cpu_cycle, &state->pending_operation,
        &state->cpu_result);
    if (*cycle_result != MIPSNES2_CPU_CYCLE_READY &&
        *cycle_result != MIPSNES2_CPU_CYCLE_COMPLETE) {
        return *cycle_result == MIPSNES2_CPU_CYCLE_STALE_OPERATION
                   ? MIPSNES2_CPU_TIMING_STALE_STATE
                   : MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    state->operation_pending = false;

    if (state->pending_operation.kind == MIPSNES2_CPU_OPERATION_INTERNAL) {
        if (mipsnes2_snes_bus_account_internal_cycles(bus, UINT64_C(1)) !=
            MIPSNES2_SNES_BUS_OK) {
            return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
        }
    } else {
        expected_accesses = UINT64_C(1);
    }

    if (bus->timing_overflow ||
        bus->access_count != accesses_before + expected_accesses ||
        bus->master_clocks !=
            clocks_before + state->operation_master_clocks) {
        return bus->timing_overflow
                   ? MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW
                   : MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
    }
    if (state->reset_sequence) {
        if (*cycle_result == MIPSNES2_CPU_CYCLE_READY &&
            cpu->instruction_cycles != cycles_before) {
            return MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
        }
        if (*cycle_result == MIPSNES2_CPU_CYCLE_COMPLETE &&
            cpu->instruction_cycles != 0U) {
            return MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
        }
    } else if (cpu->instruction_cycles != cycles_before + expected_cycles) {
        return MIPSNES2_CPU_TIMING_ACCESS_CYCLE_MISMATCH;
    }
    return MIPSNES2_CPU_TIMING_OK;
}

static void bind_resumable_state(
    mipsnes2_cpu_timing_state *state,
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines)
{
    state->bound_cpu = cpu;
    state->bound_bus = bus;
    state->bound_scheduler = scheduler;
    state->bound_hv_timing = hv_timing;
    state->bound_interrupt_lines = interrupt_lines;
    state->processor_cycles_before = cpu->instruction_cycles;
    state->bus_accesses_before = bus->access_count;
    state->bus_clocks_before = bus->master_clocks;
    state->domain_time = scheduler->now;
    state->events_dispatched = 0U;
    state->cpu_result = MIPSNES2_CPU_NOT_READY;
    state->nmi_delivered = false;
    state->irq_level_delivered = false;
    state->operation_pending = false;
    state->reset_sequence = state->reset_requested;
    state->reset_requested = false;
    state->active = true;
}

static mipsnes2_cpu_timing_result resumable_timed_step(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    size_t remaining_budget = event_budget;
    size_t dispatched = 0U;
    mipsnes2_cpu_timing_result result;

    initialize_report(report);
    result = validate_resumable_context(
        cpu, bus, scheduler, hv_timing, interrupt_lines, state,
        requested_horizon, report);
    if (result != MIPSNES2_CPU_TIMING_OK) {
        if (report != NULL && scheduler != NULL) {
            report->target_time = scheduler->now;
        }
        return result;
    }

    if (!state->active) {
        result = run_until(scheduler, hv_timing, bus->cpu_io_state,
                           scheduler->now, remaining_budget,
                           &dispatched, report);
        report->events_dispatched = dispatched;
        report->target_time = scheduler->now;
        if (result != MIPSNES2_CPU_TIMING_OK) {
            return result;
        }
        remaining_budget -= dispatched;
        bind_resumable_state(state, cpu, bus, scheduler, hv_timing,
                             interrupt_lines);
        state->events_dispatched = dispatched;
        sample_interrupt_lines(state, cpu, bus, interrupt_lines, true);
        if ((state->reset_sequence
                 ? mipsnes2_cpu_cycle_begin_reset(&state->cpu_cycle)
                 : mipsnes2_cpu_cycle_begin(&state->cpu_cycle)) !=
            MIPSNES2_CPU_CYCLE_READY) {
            state->active = false;
            fill_resumable_report(state, cpu, bus, scheduler, report);
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
    } else {
        result = run_resumable_until(
            cpu, bus, scheduler, hv_timing, interrupt_lines, state,
            scheduler->now, &remaining_budget, report);
        if (result != MIPSNES2_CPU_TIMING_OK) {
            fill_resumable_report(state, cpu, bus, scheduler, report);
            return result;
        }
    }

    for (;;) {
        mipsnes2_cpu_cycle_result cycle_result;
        mipsnes2_cpu_operation operation;

        if (state->operation_pending) {
            mipsnes2_master_clock target;

            result = validate_pending_operation(cpu, bus, scheduler, state);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
            target = state->operation_end_time < requested_horizon
                         ? state->operation_end_time
                         : requested_horizon;
            result = run_resumable_until(
                cpu, bus, scheduler, hv_timing, interrupt_lines, state,
                target, &remaining_budget, report);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
            if (scheduler->now < state->operation_end_time) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return MIPSNES2_CPU_TIMING_HORIZON_REACHED;
            }
            result = commit_pending_operation(
                cpu, bus, state, &cycle_result);
        } else {
            cycle_result = mipsnes2_cpu_cycle_peek(
                cpu, &state->cpu_cycle, &operation);
            if (cycle_result != MIPSNES2_CPU_CYCLE_READY) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return cycle_result == MIPSNES2_CPU_CYCLE_COMPLETE
                           ? MIPSNES2_CPU_TIMING_STALE_STATE
                           : MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
            }
            result = operation_clock_cost(bus, &operation,
                                          &state->operation_master_clocks);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
            if (operation.kind == MIPSNES2_CPU_OPERATION_CONTROL) {
                result = commit_control_operation(
                    cpu, bus, state, &operation, &cycle_result);
            } else {
                result = prepare_pending_operation(
                    cpu, bus, scheduler, state, &operation);
                if (result == MIPSNES2_CPU_TIMING_OK) {
                    continue;
                }
            }
        }

        if (result != MIPSNES2_CPU_TIMING_OK) {
            fill_resumable_report(state, cpu, bus, scheduler, report);
            return result;
        }
        if (cycle_result == MIPSNES2_CPU_CYCLE_COMPLETE) {
            sample_interrupt_lines(
                state, cpu, bus, interrupt_lines, true);
            fill_resumable_report(state, cpu, bus, scheduler, report);
            state->active = false;
            state->operation_pending = false;
            return MIPSNES2_CPU_TIMING_OK;
        }
    }
}

void mipsnes2_cpu_timing_state_init(mipsnes2_cpu_timing_state *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
        mipsnes2_cpu_cycle_state_init(&state->cpu_cycle);
        state->cpu_result = MIPSNES2_CPU_NOT_READY;
    }
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timing_state_begin_reset(
    mipsnes2_cpu_timing_state *state)
{
    if (state == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (state->active || state->operation_pending || state->cpu_cycle.active) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    state->reset_requested = true;
    return MIPSNES2_CPU_TIMING_OK;
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_resume(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    return resumable_timed_step(
        cpu, bus, scheduler, NULL, NULL, state, requested_horizon,
        event_budget, report);
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_resume_hv(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    if (hv_timing == NULL) {
        initialize_report(report);
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    return resumable_timed_step(
        cpu, bus, scheduler, hv_timing, NULL, state, requested_horizon,
        event_budget, report);
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timed_resume_hv_interrupts(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report)
{
    if (hv_timing == NULL || interrupt_lines == NULL) {
        initialize_report(report);
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    return resumable_timed_step(
        cpu, bus, scheduler, hv_timing, interrupt_lines, state,
        requested_horizon, event_budget, report);
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
    if (bus->ppu_counters != NULL && bus->hv_timing != hv_timing) {
        return MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH;
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
