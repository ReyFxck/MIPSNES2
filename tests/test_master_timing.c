/* MIPSNES2 shared master-clock domain tests. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "core/bus/cpu_io.h"
#include "core/scheduler/hv_timing.h"
#include "core/scheduler/master_timing.h"
#include "core/scheduler/scheduler.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef struct {
    mipsnes2_scheduler *scheduler;
    mipsnes2_hv_timing *hv_timing;
    mipsnes2_cpu_io *cpu_io;
    unsigned calls;
    mipsnes2_master_clock timestamp;
    uint16_t observed_h;
    bool observed_irq;
} observation;

typedef struct {
    observation *parent_observation;
    observation *child_observation;
    mipsnes2_scheduler_result schedule_result;
} insertion;

static void observe(void *opaque,
                    mipsnes2_master_clock timestamp,
                    mipsnes2_scheduler_token token)
{
    observation *state = opaque;

    (void)token;
    state->calls++;
    state->timestamp = timestamp;
    state->observed_h = state->hv_timing->h_master_clock;
    state->observed_irq = state->cpu_io->irq_flag;
}

static void insert_same_time(void *opaque,
                             mipsnes2_master_clock timestamp,
                             mipsnes2_scheduler_token token)
{
    insertion *state = opaque;

    observe(state->parent_observation, timestamp, token);
    state->schedule_result = mipsnes2_scheduler_schedule(
        state->parent_observation->scheduler, timestamp, 0U, observe,
        state->child_observation, NULL);
}

static int initialize_domain(mipsnes2_scheduler *scheduler,
                             mipsnes2_hv_timing *hv_timing,
                             mipsnes2_cpu_io *cpu_io)
{
    mipsnes2_scheduler_init(scheduler);
    mipsnes2_cpu_io_init(cpu_io, UINT8_C(2));
    CHECK(mipsnes2_hv_timing_init(hv_timing,
                                  MIPSNES2_HV_STANDARD_NTSC,
                                  false, cpu_io) == MIPSNES2_HV_OK);
    return 0;
}

static int test_validation_and_plain_advance(void)
{
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_master_timing_report report;

    CHECK(initialize_domain(&scheduler, &hv_timing, &cpu_io) == 0);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(100), 0U,
              &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(scheduler.now == UINT64_C(100));
    CHECK(hv_timing.elapsed_master_clocks == UINT64_C(100));
    CHECK(hv_timing.h_master_clock == UINT16_C(100));
    CHECK(report.requested_time == UINT64_C(100));
    CHECK(report.events_dispatched == 0U);
    CHECK(report.scheduler_result == MIPSNES2_SCHEDULER_OK);
    CHECK(report.hv_result == MIPSNES2_HV_OK);

    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(99), 0U,
              &report) == MIPSNES2_MASTER_TIMING_TIME_REGRESSION);
    CHECK(scheduler.now == UINT64_C(100));
    CHECK(hv_timing.elapsed_master_clocks == UINT64_C(100));

    hv_timing.elapsed_master_clocks++;
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(101), 0U,
              &report) == MIPSNES2_MASTER_TIMING_DOMAIN_MISMATCH);
    CHECK(scheduler.now == UINT64_C(100));
    CHECK(hv_timing.h_master_clock == UINT16_C(100));

    CHECK(mipsnes2_master_timing_run_until(
              NULL, &hv_timing, &cpu_io, UINT64_C(101), 0U,
              &report) == MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, NULL, &cpu_io, UINT64_C(101), 0U,
              &report) == MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, NULL, UINT64_C(101), 0U,
              &report) == MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(101), 0U,
              NULL) == MIPSNES2_MASTER_TIMING_INVALID_ARGUMENT);
    return 0;
}

static int test_hv_precedes_same_timestamp_callback(void)
{
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_master_timing_report report;
    observation state = {&scheduler, &hv_timing, &cpu_io, 0U, 0U, 0U,
                         false};

    CHECK(initialize_domain(&scheduler, &hv_timing, &cpu_io) == 0);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4207), UINT8_C(10)));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4208), 0U));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(0x10)));
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(54), 0U,
                                      observe, &state, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(100), 1U,
              &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(state.calls == 1U);
    CHECK(state.timestamp == UINT64_C(54));
    CHECK(state.observed_h == UINT16_C(54));
    CHECK(state.observed_irq);
    CHECK(report.events_dispatched == 1U);
    CHECK(scheduler.now == UINT64_C(100));
    CHECK(hv_timing.elapsed_master_clocks == UINT64_C(100));
    return 0;
}

static int test_event_budget_and_same_time_insertion(void)
{
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_master_timing_report report;
    observation parent = {&scheduler, &hv_timing, &cpu_io, 0U, 0U, 0U,
                          false};
    observation child = {&scheduler, &hv_timing, &cpu_io, 0U, 0U, 0U,
                         false};
    insertion state = {&parent, &child,
                       MIPSNES2_SCHEDULER_INVALID_ARGUMENT};

    CHECK(initialize_domain(&scheduler, &hv_timing, &cpu_io) == 0);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(100), 0U,
                                      insert_same_time, &state, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(200), 0U,
              &report) == MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result ==
          MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(scheduler.now == 0U && hv_timing.elapsed_master_clocks == 0U);
    CHECK(parent.calls == 0U);

    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(200), 1U,
              &report) == MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result ==
          MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(report.events_dispatched == 1U);
    CHECK(state.schedule_result == MIPSNES2_SCHEDULER_OK);
    CHECK(parent.calls == 1U && child.calls == 0U);
    CHECK(scheduler.now == UINT64_C(100));
    CHECK(hv_timing.elapsed_master_clocks == UINT64_C(100));

    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(200), 1U,
              &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(report.events_dispatched == 1U);
    CHECK(child.calls == 1U && child.timestamp == UINT64_C(100));
    CHECK(child.observed_h == UINT16_C(100));
    CHECK(scheduler.now == UINT64_C(200));
    CHECK(hv_timing.elapsed_master_clocks == UINT64_C(200));
    return 0;
}

static int test_failures_do_not_desynchronize_valid_domain(void)
{
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_master_timing_report report;

    CHECK(initialize_domain(&scheduler, &hv_timing, &cpu_io) == 0);
    scheduler.dispatching = true;
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(10), 0U,
              &report) == MIPSNES2_MASTER_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result == MIPSNES2_SCHEDULER_REENTRANT_RUN);
    CHECK(scheduler.now == 0U && hv_timing.elapsed_master_clocks == 0U);
    scheduler.dispatching = false;

    hv_timing.h_master_clock = MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, UINT64_C(10), 0U,
              &report) == MIPSNES2_MASTER_TIMING_HV_ERROR);
    CHECK(report.hv_result == MIPSNES2_HV_INVALID_ARGUMENT);
    CHECK(scheduler.now == 0U && hv_timing.elapsed_master_clocks == 0U);

    CHECK(initialize_domain(&scheduler, &hv_timing, &cpu_io) == 0);
    hv_timing.frame_count = UINT64_MAX;
    hv_timing.v_counter = (uint16_t)(MIPSNES2_HV_NTSC_SCANLINES - 1U);
    hv_timing.h_master_clock =
        (uint16_t)(MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS - 1U);
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, 1U, 0U,
              &report) == MIPSNES2_MASTER_TIMING_HV_ERROR);
    CHECK(report.hv_result == MIPSNES2_HV_TIME_OVERFLOW);
    CHECK(scheduler.now == 0U && hv_timing.elapsed_master_clocks == 0U);
    CHECK(hv_timing.frame_count == UINT64_MAX);
    CHECK(hv_timing.v_counter ==
          (uint16_t)(MIPSNES2_HV_NTSC_SCANLINES - 1U));
    CHECK(hv_timing.h_master_clock ==
          (uint16_t)(MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS - 1U));
    return 0;
}

static int test_chunked_determinism(void)
{
    mipsnes2_scheduler one_scheduler;
    mipsnes2_scheduler chunked_scheduler;
    mipsnes2_hv_timing one_hv;
    mipsnes2_hv_timing chunked_hv;
    mipsnes2_cpu_io one_io;
    mipsnes2_cpu_io chunked_io;
    mipsnes2_master_timing_report report;
    const uint64_t target = UINT64_C(2778);

    CHECK(initialize_domain(&one_scheduler, &one_hv, &one_io) == 0);
    CHECK(initialize_domain(&chunked_scheduler, &chunked_hv,
                            &chunked_io) == 0);
    CHECK(mipsnes2_master_timing_run_until(
              &one_scheduler, &one_hv, &one_io, target, 0U,
              &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(mipsnes2_master_timing_run_until(
              &chunked_scheduler, &chunked_hv, &chunked_io,
              UINT64_C(4), 0U, &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(mipsnes2_master_timing_run_until(
              &chunked_scheduler, &chunked_hv, &chunked_io,
              UINT64_C(1096), 0U, &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(mipsnes2_master_timing_run_until(
              &chunked_scheduler, &chunked_hv, &chunked_io,
              UINT64_C(1364), 0U, &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(mipsnes2_master_timing_run_until(
              &chunked_scheduler, &chunked_hv, &chunked_io, target, 0U,
              &report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(one_scheduler.now == chunked_scheduler.now);
    CHECK(one_hv.elapsed_master_clocks == chunked_hv.elapsed_master_clocks);
    CHECK(one_hv.frame_count == chunked_hv.frame_count);
    CHECK(one_hv.h_master_clock == chunked_hv.h_master_clock);
    CHECK(one_hv.v_counter == chunked_hv.v_counter);
    CHECK(one_hv.field == chunked_hv.field);
    CHECK(one_io.hblank == chunked_io.hblank);
    CHECK(one_io.vblank == chunked_io.vblank);
    CHECK(one_io.nmi_flag == chunked_io.nmi_flag);
    CHECK(one_io.irq_flag == chunked_io.irq_flag);
    return 0;
}

int main(void)
{
    CHECK(test_validation_and_plain_advance() == 0);
    CHECK(test_hv_precedes_same_timestamp_callback() == 0);
    CHECK(test_event_budget_and_same_time_insertion() == 0);
    CHECK(test_failures_do_not_desynchronize_valid_domain() == 0);
    CHECK(test_chunked_determinism() == 0);
    (void)puts("PASS: shared scheduler/H-V master-clock domain");
    return 0;
}
