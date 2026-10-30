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

static void inhibited_cpu_write(void *context, mipsnes2_address address,
                                uint8_t value)
{
    (void)mipsnes2_snes_bus_account_inhibited_write(
        (mipsnes2_snes_bus *)context, address, value);
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
    report->dma_master_clocks = state->dma_master_clocks;
    report->dma_bytes = state->dma_bytes;
    report->hdma_master_clocks = state->hdma_master_clocks;
    report->hdma_table_bytes = state->hdma_table_bytes;
    report->hdma_data_bytes = state->hdma_data_bytes;
    report->refresh_master_clocks = state->refresh_master_clocks;

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
            bus->master_clocks - state->bus_clocks_before +
            state->dma_master_clocks + state->hdma_master_clocks +
            state->refresh_master_clocks;
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
    if (drive_cpu && bus->dma_request_mask == 0U && !state->dma_active) {
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

static bool committed_clocks_fit(const mipsnes2_cpu_timing_state *state,
                                 const mipsnes2_snes_bus *bus,
                                 uint64_t extra)
{
    uint64_t total;

    if (bus->master_clocks < state->bus_clocks_before) {
        return false;
    }
    total = bus->master_clocks - state->bus_clocks_before;
    if (total > UINT64_MAX - state->dma_master_clocks) {
        return false;
    }
    total += state->dma_master_clocks;
    if (total > UINT64_MAX - state->hdma_master_clocks) {
        return false;
    }
    total += state->hdma_master_clocks;
    if (total > UINT64_MAX - state->refresh_master_clocks) {
        return false;
    }
    total += state->refresh_master_clocks;
    return total <= UINT64_MAX - extra;
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

enum {
    WRAM_REFRESH_NOMINAL_H_CLOCK = 536,
    WRAM_REFRESH_ALIGNMENT_PHASE = 2,
    WRAM_REFRESH_MASTER_CLOCKS = 40
};

static mipsnes2_cpu_timing_result refresh_for_line(
    mipsnes2_master_clock line_start,
    mipsnes2_master_clock *refresh_start,
    mipsnes2_master_clock *refresh_end)
{
    mipsnes2_master_clock nominal;
    mipsnes2_master_clock lower;
    mipsnes2_master_clock upper;
    mipsnes2_master_clock remainder;

    if (line_start > UINT64_MAX - WRAM_REFRESH_NOMINAL_H_CLOCK) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    nominal = line_start + WRAM_REFRESH_NOMINAL_H_CLOCK;
    remainder = (nominal - WRAM_REFRESH_ALIGNMENT_PHASE) & UINT64_C(7);
    lower = nominal - remainder;
    if (lower > UINT64_MAX - 8U) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    upper = lower + 8U;
    *refresh_start = nominal - lower <= upper - nominal ? lower : upper;
    if (*refresh_start > UINT64_MAX - WRAM_REFRESH_MASTER_CLOCKS) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    *refresh_end = *refresh_start + WRAM_REFRESH_MASTER_CLOCKS;
    return MIPSNES2_CPU_TIMING_OK;
}

/* Refresh begins near H=536, with successive starts retaining the initial
 * absolute modulo-eight phase. Normal 1364-clock lines therefore alternate
 * H=538/H=534; a 1360-clock short line retains the current phase. */
static mipsnes2_cpu_timing_result next_refresh_window(
    const mipsnes2_hv_timing *hv_timing,
    mipsnes2_master_clock now,
    mipsnes2_master_clock *refresh_start,
    mipsnes2_master_clock *refresh_end)
{
    mipsnes2_master_clock line_start;
    uint16_t line_clocks;
    mipsnes2_cpu_timing_result result;

    if (hv_timing == NULL || refresh_start == NULL || refresh_end == NULL ||
        now != hv_timing->elapsed_master_clocks ||
        now < hv_timing->h_master_clock) {
        return MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH;
    }
    line_start = now - hv_timing->h_master_clock;
    result = refresh_for_line(line_start, refresh_start, refresh_end);
    if (result != MIPSNES2_CPU_TIMING_OK || now < *refresh_end) {
        return result;
    }
    line_clocks = mipsnes2_hv_scanline_master_clocks(hv_timing);
    if (line_clocks == 0U || line_start > UINT64_MAX - line_clocks) {
        return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
    }
    return refresh_for_line(line_start + line_clocks,
                            refresh_start, refresh_end);
}

static mipsnes2_cpu_timing_result run_phase_with_refresh(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock *phase_end,
    mipsnes2_master_clock horizon,
    size_t *remaining_budget,
    mipsnes2_cpu_timing_report *report)
{
    for (;;) {
        mipsnes2_master_clock target;
        mipsnes2_master_clock refresh_start;
        mipsnes2_master_clock refresh_end;
        mipsnes2_cpu_timing_result result;

        if (hv_timing == NULL) {
            target = *phase_end < horizon ? *phase_end : horizon;
            return run_resumable_until(
                cpu, bus, scheduler, NULL, interrupt_lines, state,
                target, remaining_budget, report);
        }
        if (state->refresh_stall_active) {
            target = state->refresh_stall_end_time < horizon
                         ? state->refresh_stall_end_time : horizon;
            result = run_resumable_until(
                cpu, bus, scheduler, hv_timing, interrupt_lines, state,
                target, remaining_budget, report);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                return result;
            }
            if (scheduler->now < state->refresh_stall_end_time) {
                return MIPSNES2_CPU_TIMING_OK;
            }
            state->refresh_master_clocks += state->refresh_phase_clocks;
            state->refresh_phase_clocks = 0U;
            state->refresh_stall_active = false;
            continue;
        }

        result = next_refresh_window(
            hv_timing, scheduler->now, &refresh_start, &refresh_end);
        if (result != MIPSNES2_CPU_TIMING_OK) {
            return result;
        }
        if (refresh_start >= *phase_end) {
            target = *phase_end < horizon ? *phase_end : horizon;
            return run_resumable_until(
                cpu, bus, scheduler, hv_timing, interrupt_lines, state,
                target, remaining_budget, report);
        }
        if (scheduler->now < refresh_start) {
            target = refresh_start < horizon ? refresh_start : horizon;
            result = run_resumable_until(
                cpu, bus, scheduler, hv_timing, interrupt_lines, state,
                target, remaining_budget, report);
            if (result != MIPSNES2_CPU_TIMING_OK ||
                scheduler->now < refresh_start) {
                return result;
            }
        }
        {
            uint64_t delay = refresh_end - scheduler->now;
            if (*phase_end > UINT64_MAX - delay ||
                state->refresh_master_clocks > UINT64_MAX - delay ||
                !committed_clocks_fit(state, bus, delay)) {
                return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
            }
            *phase_end += delay;
            state->refresh_stall_end_time = refresh_end;
            state->refresh_phase_clocks = (uint8_t)delay;
            state->refresh_stall_active = true;
        }
    }
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
        scheduler->now > UINT64_MAX - clocks ||
        !committed_clocks_fit(state, bus, clocks)) {
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
    bool fetch_candidate = !state->reset_sequence &&
        !state->abort_opcode_fetched &&
        state->pending_operation.kind == MIPSNES2_CPU_OPERATION_READ &&
        cpu->instruction_cycles == state->processor_cycles_before &&
        state->pending_operation.address ==
            mipsnes2_address_make(cpu->pbr, cpu->pc);
    mipsnes2_cpu_timing_result result;
    void (*original_write)(void *, mipsnes2_address, uint8_t) =
        cpu->bus.write8;
    bool inhibited = state->abort_latched &&
        state->pending_operation.kind == MIPSNES2_CPU_OPERATION_WRITE &&
        state->abort_write_disposition == MIPSNES2_CPU_ABORT_WRITE_INHIBIT;
    bool dma_grace = bus->dma_request_mask != 0U &&
                      bus->dma_grace_cycles != 0U;
    bool requests_dma = state->pending_operation.kind ==
                            MIPSNES2_CPU_OPERATION_WRITE &&
        (state->pending_operation.address & UINT32_C(0x40ffff)) ==
            UINT32_C(0x00420b);

    result = validate_pending_operation(
        cpu, bus, state->bound_scheduler, state);
    if (result != MIPSNES2_CPU_TIMING_OK) {
        return result;
    }
    if (fetch_candidate) {
        state->abort_cpu_checkpoint = *cpu;
    }
    if (inhibited) {
        cpu->bus.write8 = inhibited_cpu_write;
    }
    *cycle_result = mipsnes2_cpu_cycle_commit(
        cpu, &state->cpu_cycle, &state->pending_operation,
        &state->cpu_result);
    cpu->bus.write8 = original_write;
    if (*cycle_result != MIPSNES2_CPU_CYCLE_READY &&
        *cycle_result != MIPSNES2_CPU_CYCLE_COMPLETE) {
        return *cycle_result == MIPSNES2_CPU_CYCLE_STALE_OPERATION
                   ? MIPSNES2_CPU_TIMING_STALE_STATE
                   : MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    state->operation_pending = false;
    if (fetch_candidate &&
        cpu->last_opcode_address == state->pending_operation.address &&
        cpu->instruction_cycles == cycles_before + UINT64_C(1)) {
        state->abort_opcode_fetched = true;
        state->abort_checkpoint_valid = true;
    }
    if (state->pending_operation.kind == MIPSNES2_CPU_OPERATION_WRITE) {
        state->abort_write_committed = true;
        state->abort_write_disposition = MIPSNES2_CPU_ABORT_WRITE_BLOCK;
    }

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
    if (dma_grace && !requests_dma && bus->dma_grace_cycles != 0U) {
        --bus->dma_grace_cycles;
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
    state->dma_active = false;
    state->dma_phase_pending = false;
    state->dma_master_clocks = 0U;
    state->dma_bytes = 0U;
    state->hdma_master_clocks = 0U;
    state->hdma_table_bytes = 0U;
    state->hdma_data_bytes = 0U;
    state->hdma_active = false;
    state->hdma_phase_pending = false;
    state->refresh_master_clocks = 0U;
    state->refresh_phase_clocks = 0U;
    state->refresh_stall_active = false;
    state->reset_sequence = state->reset_requested;
    state->reset_requested = false;
    state->abort_opcode_fetched = false;
    state->abort_checkpoint_valid = false;
    state->abort_latched = false;
    state->abort_write_committed = false;
    state->abort_preserve_rmw_status = false;
    state->abort_status_at_pulse = 0U;
    state->abort_write_disposition = MIPSNES2_CPU_ABORT_WRITE_BLOCK;
    state->active = true;
}

static void restore_aborted_registers(mipsnes2_cpu *cpu,
                                      const mipsnes2_cpu *checkpoint,
                                      const mipsnes2_cpu_timing_state *state)
{
    cpu->a = checkpoint->a;
    cpu->x = checkpoint->x;
    cpu->y = checkpoint->y;
    cpu->s = checkpoint->s;
    cpu->d = checkpoint->d;
    cpu->pc = checkpoint->pc;
    cpu->dbr = checkpoint->dbr;
    cpu->pbr = checkpoint->pbr;
    cpu->p = state->abort_preserve_rmw_status
                 ? state->abort_status_at_pulse : checkpoint->p;
    cpu->emulation = checkpoint->emulation;
    cpu->waiting = checkpoint->waiting;
    cpu->clock_stopped = checkpoint->clock_stopped;
    mipsnes2_cpu_pulse_abort(cpu);
}

enum { DMA_INIT = 1, DMA_CHANNEL, DMA_BYTE, DMA_TAIL };

static void dma_byte(void *context, mipsnes2_address a_address,
                      mipsnes2_address b_address, bool b_to_a)
{
    mipsnes2_snes_bus_dma_byte(context, a_address, b_address, b_to_a);
}

static bool hdma_requested(const mipsnes2_snes_bus *bus)
{
    return bus->hdma_state != NULL &&
           (bus->hdma_frame_pending || bus->hdma_line_pending);
}

typedef struct {
    size_t events_dispatched;
    bool nmi_delivered;
    bool irq_level_delivered;
} mipsnes2_wait_idle_state;

static void sample_waiting_interrupt_lines(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_wait_idle_state *idle)
{
    bool delivered = false;

    if (interrupt_lines == NULL) {
        return;
    }
    (void)mipsnes2_interrupt_lines_sample_cpu_io(
        interrupt_lines, bus->cpu_io_state);
    if (bus->dma_request_mask == 0U) {
        (void)mipsnes2_interrupt_lines_drive_cpu(
            interrupt_lines, cpu, &delivered);
        idle->nmi_delivered = idle->nmi_delivered || delivered;
        idle->irq_level_delivered = cpu->irq_level;
    }
}

static void fill_wait_idle_report(
    const mipsnes2_wait_idle_state *idle,
    const mipsnes2_scheduler *scheduler,
    mipsnes2_cpu_timing_report *report)
{
    report->cpu_result = MIPSNES2_CPU_WAITING;
    report->events_dispatched = idle->events_dispatched;
    report->nmi_delivered = idle->nmi_delivered;
    report->irq_level_delivered = idle->irq_level_delivered;
    report->target_time = scheduler->now;
}

static mipsnes2_cpu_timing_result idle_waiting_cpu_until(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_master_clock requested_horizon,
    size_t *remaining_budget,
    mipsnes2_wait_idle_state *idle,
    mipsnes2_cpu_timing_report *report)
{
    sample_waiting_interrupt_lines(cpu, bus, interrupt_lines, idle);
    for (;;) {
        mipsnes2_master_clock target = requested_horizon;
        mipsnes2_master_clock next_event;
        size_t dispatched = 0U;
        mipsnes2_cpu_timing_result result;

        if (cpu->nmi_pending || cpu->irq_level || hdma_requested(bus)) {
            return MIPSNES2_CPU_TIMING_OK;
        }
        if (scheduler->now == requested_horizon) {
            fill_wait_idle_report(idle, scheduler, report);
            return MIPSNES2_CPU_TIMING_HORIZON_REACHED;
        }
        if (mipsnes2_scheduler_next_event_time(scheduler, &next_event) &&
            next_event < target) {
            target = next_event;
        }
        if (hv_timing != NULL && interrupt_lines != NULL) {
            uint64_t clocks;

            if (!mipsnes2_hv_clocks_until_next_boundary(
                    hv_timing, bus->cpu_io_state, &clocks)) {
                report->hv_result = MIPSNES2_HV_INVALID_ARGUMENT;
                fill_wait_idle_report(idle, scheduler, report);
                return MIPSNES2_CPU_TIMING_HV_ERROR;
            }
            if (clocks > UINT64_MAX - scheduler->now) {
                fill_wait_idle_report(idle, scheduler, report);
                return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
            }
            if (scheduler->now + clocks < target) {
                target = scheduler->now + clocks;
            }
        }

        result = run_until(scheduler, hv_timing, bus->cpu_io_state,
                           target, *remaining_budget, &dispatched, report);
        if (idle->events_dispatched > SIZE_MAX - dispatched) {
            fill_wait_idle_report(idle, scheduler, report);
            return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
        }
        idle->events_dispatched += dispatched;
        if (dispatched <= *remaining_budget) {
            *remaining_budget -= dispatched;
        } else {
            *remaining_budget = 0U;
        }
        sample_waiting_interrupt_lines(cpu, bus, interrupt_lines, idle);
        if (result != MIPSNES2_CPU_TIMING_OK) {
            fill_wait_idle_report(idle, scheduler, report);
            return result;
        }
    }
}

static mipsnes2_cpu_timing_result resume_hdma(
    mipsnes2_cpu *cpu, mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler, mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state, mipsnes2_master_clock horizon,
    size_t *event_budget, mipsnes2_cpu_timing_report *report)
{
    if (!state->hdma_active) {
        mipsnes2_hdma_cursor_result start;

        if (!hdma_requested(bus) || bus->dma_registers == NULL) {
            return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
        }
        if (bus->hdma_frame_pending) {
            bus->hdma_frame_pending = false;
            start = mipsnes2_hdma_cursor_begin_frame(
                &state->hdma_cursor, bus->hdma_state, bus->dma_registers,
                bus->hdma_enable_mask);
        } else {
            bus->hdma_line_pending = false;
            start = mipsnes2_hdma_cursor_begin_scanline(
                &state->hdma_cursor, bus->hdma_state,
                bus->hdma_enable_mask);
        }
        if (start == MIPSNES2_HDMA_CURSOR_COMPLETE) {
            return MIPSNES2_CPU_TIMING_OK;
        }
        if (start != MIPSNES2_HDMA_CURSOR_READY) {
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
        if (state->dma_active) {
            uint8_t current = state->dma_cursor.channel;
            uint8_t cancelled;

            if (!mipsnes2_dma_cursor_cancel_channels(
                    &state->dma_cursor, state->hdma_cursor.selected_mask,
                    &cancelled)) {
                return MIPSNES2_CPU_TIMING_STALE_STATE;
            }
            if (current < 8U &&
                (cancelled & (uint8_t)(1U << current)) != 0U) {
                if (state->dma_cursor.pending_mask == 0U) {
                    state->dma_phase = DMA_TAIL;
                } else if (state->dma_phase == DMA_BYTE) {
                    state->dma_phase = DMA_CHANNEL;
                }
            }
        }
        state->hdma_active = true;
    }
    while (state->hdma_active) {
        mipsnes2_hdma_cursor_result cursor_result;
        mipsnes2_cpu_timing_result result;

        if (!state->hdma_phase_pending) {
            cursor_result = mipsnes2_hdma_cursor_peek(
                &state->hdma_cursor, &state->hdma_operation);
            if (cursor_result != MIPSNES2_HDMA_CURSOR_READY) {
                return MIPSNES2_CPU_TIMING_STALE_STATE;
            }
            if (scheduler->now > UINT64_MAX -
                                      state->hdma_operation.master_clocks ||
                !committed_clocks_fit(
                    state, bus, state->hdma_operation.master_clocks)) {
                return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
            }
            state->hdma_end_time = scheduler->now +
                                   state->hdma_operation.master_clocks;
            (void)memcpy(&state->pending_cpu_snapshot, cpu, sizeof(*cpu));
            state->pending_bus_accesses = bus->access_count;
            state->pending_bus_clocks = bus->master_clocks;
            state->pending_fastrom_enabled = bus->fastrom_enabled;
            state->hdma_phase_pending = true;
        }
        if (memcmp(cpu, &state->pending_cpu_snapshot, sizeof(*cpu)) != 0 ||
            bus->access_count != state->pending_bus_accesses ||
            bus->master_clocks != state->pending_bus_clocks ||
            bus->fastrom_enabled != state->pending_fastrom_enabled ||
            memcmp(bus->dma_registers,
                   &state->hdma_cursor.register_snapshot,
                   sizeof(*bus->dma_registers)) != 0 ||
            memcmp(bus->hdma_state, &state->hdma_cursor.state_snapshot,
                   sizeof(*bus->hdma_state)) != 0) {
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
        result = run_phase_with_refresh(
            cpu, bus, scheduler, hv_timing, interrupt_lines, state,
            &state->hdma_end_time, horizon, event_budget, report);
        if (result != MIPSNES2_CPU_TIMING_OK) {
            return result;
        }
        if (scheduler->now < state->hdma_end_time) {
            return MIPSNES2_CPU_TIMING_HORIZON_REACHED;
        }
        if (memcmp(cpu, &state->pending_cpu_snapshot, sizeof(*cpu)) != 0 ||
            bus->access_count != state->pending_bus_accesses ||
            bus->master_clocks != state->pending_bus_clocks ||
            bus->fastrom_enabled != state->pending_fastrom_enabled ||
            memcmp(bus->dma_registers,
                   &state->hdma_cursor.register_snapshot,
                   sizeof(*bus->dma_registers)) != 0 ||
            memcmp(bus->hdma_state, &state->hdma_cursor.state_snapshot,
                   sizeof(*bus->hdma_state)) != 0) {
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
        {
            uint8_t value = 0U;

            if (state->hdma_operation.kind ==
                MIPSNES2_HDMA_OPERATION_TABLE_READ) {
                value = mipsnes2_snes_bus_dma_read_a(
                    bus, state->hdma_operation.a_address);
                ++state->hdma_table_bytes;
            } else if (state->hdma_operation.kind ==
                       MIPSNES2_HDMA_OPERATION_TRANSFER) {
                mipsnes2_snes_bus_dma_byte(
                    bus, state->hdma_operation.a_address,
                    state->hdma_operation.b_address,
                    state->hdma_operation.b_to_a);
                ++state->hdma_data_bytes;
            }
            cursor_result = mipsnes2_hdma_cursor_commit(
                &state->hdma_cursor, &state->hdma_operation, value);
        }
        if (cursor_result != MIPSNES2_HDMA_CURSOR_READY &&
            cursor_result != MIPSNES2_HDMA_CURSOR_COMPLETE) {
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
        state->hdma_master_clocks +=
            state->hdma_operation.master_clocks;
        state->hdma_phase_pending = false;
        state->hdma_active = cursor_result != MIPSNES2_HDMA_CURSOR_COMPLETE;
        if (bus->hdma_vblank_pending) {
            if (!mipsnes2_hdma_state_deactivate(bus->hdma_state)) {
                return MIPSNES2_CPU_TIMING_STALE_STATE;
            }
            mipsnes2_hdma_cursor_init(&state->hdma_cursor);
            bus->hdma_vblank_pending = false;
            state->hdma_active = false;
        }
    }
    return MIPSNES2_CPU_TIMING_OK;
}

static mipsnes2_cpu_timing_result resume_dma(
    mipsnes2_cpu *cpu, mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler, mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state, mipsnes2_master_clock horizon,
    size_t *event_budget, mipsnes2_cpu_timing_report *report)
{
    while (state->dma_active) {
        mipsnes2_cpu_timing_result result;

        if (!state->dma_phase_pending) {
            uint8_t clocks = 8U;

            if (state->hdma_active || hdma_requested(bus)) {
                result = resume_hdma(cpu, bus, scheduler, hv_timing,
                                     interrupt_lines, state, horizon,
                                     event_budget, report);
                if (result != MIPSNES2_CPU_TIMING_OK) {
                    return result;
                }
                continue;
            }

            if (state->dma_phase == DMA_INIT) {
                clocks = (uint8_t)(8U - (scheduler->now & 7U) + 8U);
            } else if (state->dma_phase == DMA_TAIL) {
                clocks = (uint8_t)(state->dma_cpu_clocks -
                    (scheduler->now - state->dma_start_time) %
                        state->dma_cpu_clocks);
            }
            if (scheduler->now > UINT64_MAX - clocks ||
                state->dma_master_clocks > UINT64_MAX - clocks ||
                !committed_clocks_fit(state, bus, clocks)) {
                return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
            }
            state->dma_end_time = scheduler->now + clocks;
            state->dma_phase_clocks = clocks;
            (void)memcpy(&state->pending_cpu_snapshot, cpu, sizeof(*cpu));
            state->pending_bus_accesses = bus->access_count;
            state->pending_bus_clocks = bus->master_clocks;
            state->pending_fastrom_enabled = bus->fastrom_enabled;
            state->dma_register_snapshot = *bus->dma_registers;
            state->dma_phase_pending = true;
        }
        if (state->dma_cursor.registers != bus->dma_registers ||
            bus->dma_request_mask != 0U ||
            memcmp(cpu, &state->pending_cpu_snapshot, sizeof(*cpu)) != 0 ||
            bus->access_count != state->pending_bus_accesses ||
            bus->master_clocks != state->pending_bus_clocks ||
            bus->fastrom_enabled != state->pending_fastrom_enabled ||
            memcmp(bus->dma_registers, &state->dma_register_snapshot,
                   sizeof(*bus->dma_registers)) != 0) {
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
        result = run_phase_with_refresh(
            cpu, bus, scheduler, hv_timing, interrupt_lines, state,
            &state->dma_end_time, horizon, event_budget, report);
        if (result != MIPSNES2_CPU_TIMING_OK) {
            return result;
        }
        /* Recheck after callbacks before admitting any device effect. */
        if (state->dma_cursor.registers != bus->dma_registers ||
            memcmp(cpu, &state->pending_cpu_snapshot, sizeof(*cpu)) != 0 ||
            bus->access_count != state->pending_bus_accesses ||
            bus->master_clocks != state->pending_bus_clocks ||
            bus->fastrom_enabled != state->pending_fastrom_enabled ||
            bus->dma_request_mask != 0U ||
            memcmp(bus->dma_registers, &state->dma_register_snapshot,
                   sizeof(*bus->dma_registers)) != 0) {
            return MIPSNES2_CPU_TIMING_STALE_STATE;
        }
        if (scheduler->now < state->dma_end_time) {
            return MIPSNES2_CPU_TIMING_HORIZON_REACHED;
        }
        state->dma_master_clocks += state->dma_phase_clocks;
        state->dma_phase_pending = false;
        if (state->dma_phase == DMA_INIT) {
            state->dma_phase = DMA_CHANNEL;
        } else if (state->dma_phase == DMA_CHANNEL) {
            state->dma_phase = DMA_BYTE;
        } else if (state->dma_phase == DMA_BYTE) {
            uint32_t done;
            uint8_t previous_channel = state->dma_cursor.channel;
            mipsnes2_dma_run_result dma_result = mipsnes2_dma_cursor_run(
                &state->dma_cursor, 1U, dma_byte, bus, &done);

            if (dma_result == MIPSNES2_DMA_RUN_INVALID_ARGUMENT || done != 1U) {
                return MIPSNES2_CPU_TIMING_STALE_STATE;
            }
            ++state->dma_bytes;
            state->dma_phase = dma_result == MIPSNES2_DMA_RUN_COMPLETE
                ? DMA_TAIL : (previous_channel != state->dma_cursor.channel
                    ? DMA_CHANNEL : DMA_BYTE);
        } else {
            state->dma_active = false;
        }
    }
    return MIPSNES2_CPU_TIMING_OK;
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
    mipsnes2_wait_idle_state idle = {0};
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
        if (cpu->waiting && !cpu->clock_stopped) {
            result = idle_waiting_cpu_until(
                cpu, bus, scheduler, hv_timing, interrupt_lines,
                requested_horizon, &remaining_budget, &idle, report);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                return result;
            }
        }
        result = run_until(scheduler, hv_timing, bus->cpu_io_state,
                           scheduler->now, remaining_budget,
                           &dispatched, report);
        if (idle.events_dispatched > SIZE_MAX - dispatched) {
            fill_wait_idle_report(&idle, scheduler, report);
            return MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW;
        }
        dispatched += idle.events_dispatched;
        report->events_dispatched = dispatched;
        report->target_time = scheduler->now;
        if (result != MIPSNES2_CPU_TIMING_OK) {
            return result;
        }
        remaining_budget -= dispatched - idle.events_dispatched;
        bind_resumable_state(state, cpu, bus, scheduler, hv_timing,
                             interrupt_lines);
        state->events_dispatched = dispatched;
        state->nmi_delivered = idle.nmi_delivered;
        state->irq_level_delivered = idle.irq_level_delivered;
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

        if (!state->operation_pending && !state->dma_active &&
            (state->hdma_active || hdma_requested(bus))) {
            result = resume_hdma(cpu, bus, scheduler, hv_timing,
                                 interrupt_lines, state, requested_horizon,
                                 &remaining_budget, report);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
        }

        if (state->dma_active) {
            result = resume_dma(cpu, bus, scheduler, hv_timing,
                interrupt_lines, state, requested_horizon,
                &remaining_budget, report);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
        }

        if (state->operation_pending) {
            result = validate_pending_operation(cpu, bus, scheduler, state);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
            result = run_phase_with_refresh(
                cpu, bus, scheduler, hv_timing, interrupt_lines, state,
                &state->operation_end_time, requested_horizon,
                &remaining_budget, report);
            if (result != MIPSNES2_CPU_TIMING_OK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return result;
            }
            if (scheduler->now < state->operation_end_time) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return MIPSNES2_CPU_TIMING_HORIZON_REACHED;
            }
            if (state->abort_latched &&
                state->pending_operation.kind == MIPSNES2_CPU_OPERATION_WRITE &&
                state->abort_write_disposition ==
                    MIPSNES2_CPU_ABORT_WRITE_BLOCK) {
                fill_resumable_report(state, cpu, bus, scheduler, report);
                return MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED;
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
            if (operation.kind != MIPSNES2_CPU_OPERATION_CONTROL &&
                bus->dma_request_mask != 0U &&
                bus->dma_grace_cycles == 0U) {
                if (!mipsnes2_dma_cursor_begin(&state->dma_cursor,
                    bus->dma_registers, bus->dma_request_mask)) {
                    fill_resumable_report(state, cpu, bus, scheduler, report);
                    return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
                }
                bus->dma_request_mask = 0U;
                state->dma_start_time = scheduler->now;
                state->dma_cpu_clocks = state->operation_master_clocks;
                state->dma_phase = DMA_INIT;
                state->dma_active = true;
                state->dma_phase_pending = false;
                continue;
            }
            if (operation.kind == MIPSNES2_CPU_OPERATION_CONTROL) {
                result = commit_control_operation(
                    cpu, bus, state, &operation, &cycle_result);
            } else {
                if (state->abort_latched &&
                    operation.kind == MIPSNES2_CPU_OPERATION_WRITE &&
                    state->abort_write_disposition ==
                        MIPSNES2_CPU_ABORT_WRITE_BLOCK) {
                    fill_resumable_report(state, cpu, bus, scheduler, report);
                    return MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED;
                }
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
            if (state->abort_latched &&
                state->cpu_result == MIPSNES2_CPU_OK) {
                restore_aborted_registers(
                    cpu, &state->abort_cpu_checkpoint, state);
            }
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

mipsnes2_cpu_timing_result mipsnes2_cpu_timing_pulse_abort(
    mipsnes2_cpu_timing_state *state)
{
    uint8_t opcode;

    if (state == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (!state->active || state->reset_sequence ||
        !state->abort_opcode_fetched || !state->abort_checkpoint_valid ||
        state->abort_write_committed) {
        return MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION;
    }
    opcode = state->bound_cpu->last_opcode;
    if (opcode == UINT8_C(0x00) || opcode == UINT8_C(0x02) ||
        opcode == UINT8_C(0x40) || opcode == UINT8_C(0xcb) ||
        opcode == UINT8_C(0xdb)) {
        return MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION;
    }
    if (state->abort_latched) {
        return MIPSNES2_CPU_TIMING_OK;
    }
    state->abort_latched = true;
    state->abort_preserve_rmw_status =
        state->cpu_cycle.rmw_modify_committed;
    state->abort_status_at_pulse = state->bound_cpu->p;
    return MIPSNES2_CPU_TIMING_OK;
}

mipsnes2_cpu_timing_result mipsnes2_cpu_timing_resolve_abort_write(
    mipsnes2_cpu_timing_state *state,
    mipsnes2_cpu_abort_write_disposition disposition)
{
    mipsnes2_cpu_operation operation;

    if (state == NULL ||
        (disposition != MIPSNES2_CPU_ABORT_WRITE_EXTERNAL &&
         disposition != MIPSNES2_CPU_ABORT_WRITE_INHIBIT)) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    if (!state->active || !state->abort_latched ||
        state->abort_write_disposition != MIPSNES2_CPU_ABORT_WRITE_BLOCK) {
        return MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION;
    }
    if (state->operation_pending) {
        operation = state->pending_operation;
    } else if (mipsnes2_cpu_cycle_peek(
                   state->bound_cpu, &state->cpu_cycle, &operation) !=
               MIPSNES2_CPU_CYCLE_READY) {
        return MIPSNES2_CPU_TIMING_STALE_STATE;
    }
    if (operation.kind != MIPSNES2_CPU_OPERATION_WRITE) {
        return MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION;
    }
    state->abort_write_disposition = disposition;
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
