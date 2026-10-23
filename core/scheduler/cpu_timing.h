/* MIPSNES2 CPU/scheduler timing bridge. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CPU_TIMING_H
#define MIPSNES2_CPU_TIMING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/bus/snes_bus.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/hv_timing.h"
#include "core/scheduler/interrupt_lines.h"
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
    MIPSNES2_CPU_TIMING_SCHEDULER_ERROR,
    MIPSNES2_CPU_TIMING_HORIZON_REACHED,
    MIPSNES2_CPU_TIMING_STALE_STATE,
    MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION,
    MIPSNES2_CPU_TIMING_TIME_REGRESSION
} mipsnes2_cpu_timing_result;

typedef struct {
    mipsnes2_cpu_result cpu_result;
    mipsnes2_scheduler_result scheduler_result;
    mipsnes2_hv_result hv_result;
    bool nmi_delivered;
    bool irq_level_delivered;
    uint64_t processor_cycles;
    uint64_t bus_accesses;
    uint64_t internal_cycles;
    uint64_t master_clocks;
    mipsnes2_master_clock target_time;
    size_t events_dispatched;
} mipsnes2_cpu_timing_report;

/*
 * Caller-owned state for operation-granular 5A22 timing.
 *
 * A READ, WRITE or INTERNAL operation is first admitted with a fixed 6/8/12
 * master-clock cost.  The scheduler/H-V domain may then advance through that
 * interval in several calls.  The CPU/bus effect is committed exactly once,
 * after the operation endpoint has been reached.  The fields are private to
 * cpu_timing.c; initialize them with mipsnes2_cpu_timing_state_init().
 */
typedef struct {
    mipsnes2_cpu_cycle_state cpu_cycle;
    mipsnes2_cpu_operation pending_operation;
    mipsnes2_cpu pending_cpu_snapshot;
    mipsnes2_cpu abort_cpu_checkpoint;
    mipsnes2_cpu *bound_cpu;
    mipsnes2_snes_bus *bound_bus;
    mipsnes2_scheduler *bound_scheduler;
    mipsnes2_hv_timing *bound_hv_timing;
    mipsnes2_interrupt_lines *bound_interrupt_lines;
    uint64_t processor_cycles_before;
    uint64_t bus_accesses_before;
    uint64_t bus_clocks_before;
    uint64_t pending_processor_cycles;
    uint64_t pending_bus_accesses;
    uint64_t pending_bus_clocks;
    mipsnes2_master_clock domain_time;
    mipsnes2_master_clock operation_start_time;
    mipsnes2_master_clock operation_end_time;
    size_t events_dispatched;
    mipsnes2_cpu_result cpu_result;
    uint8_t operation_master_clocks;
    bool pending_fastrom_enabled;
    bool nmi_delivered;
    bool irq_level_delivered;
    bool active;
    bool operation_pending;
    bool reset_requested;
    bool reset_sequence;
    bool abort_opcode_fetched;
    bool abort_checkpoint_valid;
    bool abort_latched;
    bool abort_write_committed;
} mipsnes2_cpu_timing_state;

void mipsnes2_cpu_timing_state_init(mipsnes2_cpu_timing_state *state);
mipsnes2_cpu_timing_result mipsnes2_cpu_timing_state_begin_reset(
    mipsnes2_cpu_timing_state *state);

/*
 * Latch a mid-instruction ABORT without modifying the active CPU descriptor.
 * Supported only after opcode fetch and before any write in the instruction.
 * If an instruction subsequently reaches a write, resume fails closed before
 * that write; external-write qualification and recovery remain future work.
 * RESET, interrupt entry, WAI/STP and RTI are not covered by this contract.
 * Callers may invoke this from a scheduler event callback.
 */
mipsnes2_cpu_timing_result mipsnes2_cpu_timing_pulse_abort(
    mipsnes2_cpu_timing_state *state);

/*
 * Advance one instruction (or an explicitly requested RESET sequence) no
 * later than the absolute requested_horizon.  OK means the sequence completed;
 * HORIZON_REACHED leaves state active and resumable.  Reports are cumulative for
 * the current instruction/RESET sequence, with target_time equal to scheduler.now;
 * the bus clock counter is charged only at commit, so it may temporarily lag time.
 * Scheduler/H-V failures
 * also leave an uncommitted operation resumable; callbacks already dispatched
 * by the scheduler are not rolled back. While an operation is in flight, callbacks
 * may update devices/interrupt lines but must not mutate CPU, bus timing/FastROM or
 * cursor state. Active-state cancellation is not supported.
 */
mipsnes2_cpu_timing_result mipsnes2_cpu_timed_resume(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report);
mipsnes2_cpu_timing_result mipsnes2_cpu_timed_resume_hv(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report);
mipsnes2_cpu_timing_result mipsnes2_cpu_timed_resume_hv_interrupts(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    mipsnes2_cpu_timing_state *state,
    mipsnes2_master_clock requested_horizon,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report);

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
mipsnes2_cpu_timing_result mipsnes2_cpu_timed_step_hv_interrupts(
    mipsnes2_cpu *cpu,
    mipsnes2_snes_bus *bus,
    mipsnes2_scheduler *scheduler,
    mipsnes2_hv_timing *hv_timing,
    mipsnes2_interrupt_lines *interrupt_lines,
    size_t event_budget,
    mipsnes2_cpu_timing_report *report);

#endif
