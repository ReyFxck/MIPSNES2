/* MIPSNES2 scheduler foundation tests. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/bus.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/5a22_timing.h"
#include "core/scheduler/scheduler.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef struct {
    char values[128];
    mipsnes2_master_clock timestamps[128];
    mipsnes2_scheduler_token tokens[128];
    size_t count;
} event_trace;

typedef struct {
    event_trace *trace;
    char value;
} record_context;

typedef struct {
    record_context record;
    mipsnes2_scheduler *scheduler;
    record_context *child;
    mipsnes2_scheduler_result schedule_result;
} schedule_context;

typedef struct {
    record_context record;
    mipsnes2_scheduler *scheduler;
    mipsnes2_scheduler_result run_result;
} reentrant_context;

typedef struct {
    record_context record;
    mipsnes2_cpu *cpu;
    bool nmi;
} interrupt_context;

static void record_event(void *opaque,
                         mipsnes2_master_clock timestamp,
                         mipsnes2_scheduler_token token)
{
    record_context *context = opaque;
    event_trace *trace = context->trace;

    if (trace->count < sizeof(trace->values)) {
        trace->values[trace->count] = context->value;
        trace->timestamps[trace->count] = timestamp;
        trace->tokens[trace->count] = token;
        trace->count++;
    }
}

static void schedule_same_time(void *opaque,
                               mipsnes2_master_clock timestamp,
                               mipsnes2_scheduler_token token)
{
    schedule_context *context = opaque;

    record_event(&context->record, timestamp, token);
    context->schedule_result = mipsnes2_scheduler_schedule(
        context->scheduler, timestamp, UINT32_C(5), record_event,
        context->child, NULL);
}

static void attempt_reentrant_run(void *opaque,
                                  mipsnes2_master_clock timestamp,
                                  mipsnes2_scheduler_token token)
{
    reentrant_context *context = opaque;
    size_t dispatched = 99U;

    record_event(&context->record, timestamp, token);
    context->run_result = mipsnes2_scheduler_run_until(
        context->scheduler, timestamp, 1U, &dispatched);
}

static void drive_interrupt(void *opaque,
                            mipsnes2_master_clock timestamp,
                            mipsnes2_scheduler_token token)
{
    interrupt_context *context = opaque;

    record_event(&context->record, timestamp, token);
    if (context->nmi) {
        mipsnes2_cpu_pulse_nmi(context->cpu);
    } else {
        mipsnes2_cpu_set_irq(context->cpu, true);
    }
}

static int test_event_order_and_horizon(void)
{
    mipsnes2_scheduler scheduler;
    event_trace trace = {{0}, {0}, {0}, 0U};
    record_context a = {&trace, 'A'};
    record_context b = {&trace, 'B'};
    record_context c = {&trace, 'C'};
    record_context d = {&trace, 'D'};
    mipsnes2_scheduler_token token_a;
    mipsnes2_master_clock next;
    size_t dispatched;

    mipsnes2_scheduler_init(&scheduler);
    CHECK(scheduler.now == 0U);
    CHECK(scheduler.event_count == 0U);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(20), 5U,
                                      record_event, &a, &token_a) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(10), 9U,
                                      record_event, &b, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(10), 1U,
                                      record_event, &c, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(10), 1U,
                                      record_event, &d, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_next_event_time(&scheduler, &next));
    CHECK(next == UINT64_C(10));
    CHECK(mipsnes2_scheduler_event_horizon(&scheduler, UINT64_C(100)) ==
          UINT64_C(10));
    CHECK(mipsnes2_scheduler_event_horizon(&scheduler, UINT64_C(5)) ==
          UINT64_C(5));

    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(9), 0U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 0U);
    CHECK(scheduler.now == UINT64_C(9));
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(8), 0U,
                                      record_event, &a, NULL) ==
          MIPSNES2_SCHEDULER_TIME_REGRESSION);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(10), 8U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 3U);
    CHECK(trace.count == 3U);
    CHECK(memcmp(trace.values, "CDB", 3U) == 0);
    CHECK(trace.timestamps[0] == UINT64_C(10));
    CHECK(trace.tokens[0] < trace.tokens[1]);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(20), 1U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 1U);
    CHECK(trace.count == 4U && trace.values[3] == 'A');
    CHECK(trace.tokens[3] == token_a);
    CHECK(!mipsnes2_scheduler_next_event_time(&scheduler, &next));
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(19), 1U,
                                       NULL) ==
          MIPSNES2_SCHEDULER_TIME_REGRESSION);
    return 0;
}

static int test_same_time_insertion_budget_cancel_and_capacity(void)
{
    mipsnes2_scheduler scheduler;
    event_trace trace = {{0}, {0}, {0}, 0U};
    record_context child = {&trace, 'C'};
    record_context sibling = {&trace, 'S'};
    record_context ordinary = {&trace, 'O'};
    schedule_context parent = {{&trace, 'P'}, &scheduler, &child,
                               MIPSNES2_SCHEDULER_INVALID_ARGUMENT};
    reentrant_context reentrant = {{&trace, 'R'}, &scheduler,
                                   MIPSNES2_SCHEDULER_OK};
    mipsnes2_scheduler_token cancelled;
    size_t dispatched;
    size_t index;

    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(30), 5U,
                                      schedule_same_time, &parent, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(30), 5U,
                                      record_event, &sibling, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(30), 3U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 3U);
    CHECK(parent.schedule_result == MIPSNES2_SCHEDULER_OK);
    CHECK(trace.count == 3U && memcmp(trace.values, "PSC", 3U) == 0);

    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(40), 0U,
                                      record_event, &ordinary,
                                      &cancelled) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_cancel(&scheduler, cancelled) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_cancel(&scheduler, cancelled) ==
          MIPSNES2_SCHEDULER_EVENT_NOT_FOUND);
    CHECK(mipsnes2_scheduler_cancel(&scheduler, 0U) ==
          MIPSNES2_SCHEDULER_INVALID_ARGUMENT);

    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(50), 0U,
                                      record_event, &ordinary, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(50), 0U,
                                      record_event, &ordinary, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(100), 1U,
                                       &dispatched) ==
          MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(dispatched == 1U);
    CHECK(scheduler.now == UINT64_C(50));
    CHECK(scheduler.event_count == 1U);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(100), 1U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 1U && scheduler.now == UINT64_C(100));

    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(110), 0U,
                                      attempt_reentrant_run, &reentrant,
                                      NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(110), 1U,
                                       NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(reentrant.run_result == MIPSNES2_SCHEDULER_REENTRANT_RUN);

    mipsnes2_scheduler_init(&scheduler);
    for (index = 0U; index < MIPSNES2_SCHEDULER_EVENT_CAPACITY; ++index) {
        CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(1), 0U,
                                          record_event, &ordinary, NULL) ==
              MIPSNES2_SCHEDULER_OK);
    }
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(1), 0U,
                                      record_event, &ordinary, NULL) ==
          MIPSNES2_SCHEDULER_CAPACITY_EXHAUSTED);
    CHECK(mipsnes2_scheduler_run_until(
              &scheduler, UINT64_C(1), MIPSNES2_SCHEDULER_EVENT_CAPACITY,
              &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == MIPSNES2_SCHEDULER_EVENT_CAPACITY);

    mipsnes2_scheduler_init(&scheduler);
    scheduler.next_token = UINT64_MAX;
    CHECK(mipsnes2_scheduler_schedule(&scheduler, 0U, 0U, record_event,
                                      &ordinary, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, 0U, 0U, record_event,
                                      &ordinary, NULL) ==
          MIPSNES2_SCHEDULER_TOKEN_EXHAUSTED);
    return 0;
}

static int test_clock_ratios(void)
{
    mipsnes2_clock_ratio ratio;
    mipsnes2_clock_ratio chunked;
    mipsnes2_master_clock converted = UINT64_C(0x55);
    mipsnes2_master_clock total = 0U;
    uint64_t index;

    CHECK(mipsnes2_clock_ratio_init(&ratio, 6U, 2U) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(ratio.numerator == 3U && ratio.denominator == 1U);
    CHECK(mipsnes2_clock_ratio_advance(&ratio, UINT64_C(7), &converted) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(converted == UINT64_C(21) && ratio.remainder == 0U);

    CHECK(mipsnes2_clock_ratio_init(&ratio, 3U, 2U) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_clock_ratio_advance(&ratio, 1U, &converted) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(converted == 1U && ratio.remainder == 1U);
    CHECK(mipsnes2_clock_ratio_advance(&ratio, 1U, &converted) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(converted == 2U && ratio.remainder == 0U);

    CHECK(mipsnes2_clock_ratio_init(&ratio, 1001U, 1000U) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_clock_ratio_init(&chunked, 1001U, 1000U) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_clock_ratio_advance(&ratio, UINT64_C(1000000),
                                       &converted) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(converted == UINT64_C(1001000));
    for (index = 0U; index < UINT64_C(1000); ++index) {
        mipsnes2_master_clock part;
        CHECK(mipsnes2_clock_ratio_advance(&chunked, UINT64_C(1000),
                                           &part) ==
              MIPSNES2_SCHEDULER_OK);
        total += part;
    }
    CHECK(total == converted && chunked.remainder == ratio.remainder);

    CHECK(mipsnes2_clock_ratio_init(NULL, 1U, 1U) ==
          MIPSNES2_SCHEDULER_INVALID_ARGUMENT);
    CHECK(mipsnes2_clock_ratio_init(&ratio, 0U, 1U) ==
          MIPSNES2_SCHEDULER_INVALID_ARGUMENT);
    CHECK(mipsnes2_clock_ratio_init(&ratio, 1U, 0U) ==
          MIPSNES2_SCHEDULER_INVALID_ARGUMENT);
    CHECK(mipsnes2_clock_ratio_init(&ratio, 2U, 1U) ==
          MIPSNES2_SCHEDULER_OK);
    converted = UINT64_C(0x55);
    CHECK(mipsnes2_clock_ratio_advance(&ratio, UINT64_MAX, &converted) ==
          MIPSNES2_SCHEDULER_TIME_OVERFLOW);
    CHECK(converted == UINT64_C(0x55) && ratio.remainder == 0U);
    return 0;
}

static int run_determinism_fixture(event_trace *trace,
                                   mipsnes2_master_clock *final_time)
{
    mipsnes2_scheduler scheduler;
    record_context a = {trace, 'A'};
    record_context b = {trace, 'B'};
    record_context c = {trace, 'C'};
    record_context d = {trace, 'D'};
    size_t dispatched;

    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(400), 8U,
                                      record_event, &a, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(100), 8U,
                                      record_event, &b, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(100), 2U,
                                      record_event, &c, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(250), 0U,
                                      record_event, &d, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(500), 4U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 4U);
    *final_time = scheduler.now;
    return 0;
}

static int test_deterministic_replay(void)
{
    event_trace first = {{0}, {0}, {0}, 0U};
    event_trace second = {{0}, {0}, {0}, 0U};
    mipsnes2_master_clock first_time;
    mipsnes2_master_clock second_time;

    CHECK(run_determinism_fixture(&first, &first_time) == 0);
    CHECK(run_determinism_fixture(&second, &second_time) == 0);
    CHECK(first_time == UINT64_C(500) && second_time == first_time);
    CHECK(first.count == 4U && second.count == first.count);
    CHECK(memcmp(first.values, second.values, first.count) == 0);
    CHECK(memcmp(first.timestamps, second.timestamps,
                 first.count * sizeof(first.timestamps[0])) == 0);
    CHECK(memcmp(first.tokens, second.tokens,
                 first.count * sizeof(first.tokens[0])) == 0);
    CHECK(memcmp(first.values, "CBDA", 4U) == 0);
    return 0;
}

static int test_5a22_access_timing(void)
{
    struct timing_case {
        uint8_t bank;
        uint16_t offset;
        bool fastrom;
        uint8_t clocks;
    } cases[] = {
        {0x00U, 0x0000U, false, 8U},
        {0x3fU, 0x1fffU, true, 8U},
        {0x00U, 0x2000U, false, 6U},
        {0x3fU, 0x3fffU, false, 6U},
        {0x00U, 0x4000U, false, 12U},
        {0x00U, 0x41ffU, true, 12U},
        {0x00U, 0x4200U, false, 6U},
        {0x3fU, 0x5fffU, false, 6U},
        {0x00U, 0x6000U, false, 8U},
        {0x3fU, 0xffffU, true, 8U},
        {0x40U, 0x0000U, true, 8U},
        {0x7fU, 0xffffU, true, 8U},
        {0x80U, 0x4000U, true, 12U},
        {0x80U, 0x8000U, false, 8U},
        {0xbfU, 0xffffU, true, 6U},
        {0xc0U, 0x0000U, false, 8U},
        {0xffU, 0xffffU, true, 6U}
    };
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        CHECK(mipsnes2_5a22_access_master_clocks(
                  mipsnes2_address_make(cases[index].bank,
                                        cases[index].offset),
                  cases[index].fastrom) == cases[index].clocks);
    }
    CHECK(mipsnes2_5a22_access_master_clocks(UINT32_C(0x01004000),
                                             false) == 12U);
    CHECK(MIPSNES2_5A22_INTERNAL_MASTER_CLOCKS == 6);
    return 0;
}

static int test_scheduled_cpu_interrupt_boundary(void)
{
    uint8_t bank00[UINT16_MAX + 1U];
    mipsnes2_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    event_trace trace = {{0}, {0}, {0}, 0U};
    interrupt_context nmi = {{&trace, 'N'}, &cpu, true};
    interrupt_context irq = {{&trace, 'I'}, &cpu, false};
    size_t dispatched;

    (void)memset(bank00, 0, sizeof(bank00));
    bank00[0xfffc] = 0x00U;
    bank00[0xfffd] = 0x80U;
    bank00[0x8000] = 0xcbU;
    bank00[0x8001] = 0xeaU;
    bank00[0xfffa] = 0x00U;
    bank00[0xfffb] = 0x90U;

    mipsnes2_bus_init(&bus, 0U);
    CHECK(mipsnes2_bus_map_direct(&bus, 0U, sizeof(bank00), bank00, true) ==
          MIPSNES2_BUS_OK);
    mipsnes2_cpu_init(&cpu, mipsnes2_bus_make_interface(&bus));
    CHECK(mipsnes2_cpu_reset(&cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.waiting);

    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(60), 10U,
                                      drive_interrupt, &nmi, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(60), 20U,
                                      drive_interrupt, &irq, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_event_horizon(&scheduler, UINT64_C(100)) ==
          UINT64_C(60));
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(59), 2U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 0U && cpu.waiting && !cpu.nmi_pending && !cpu.irq_level);
    CHECK(mipsnes2_scheduler_run_until(&scheduler, UINT64_C(60), 2U,
                                       &dispatched) == MIPSNES2_SCHEDULER_OK);
    CHECK(dispatched == 2U && trace.count == 2U);
    CHECK(memcmp(trace.values, "NI", 2U) == 0);
    CHECK(cpu.nmi_pending && cpu.irq_level);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu.waiting && !cpu.nmi_pending);
    CHECK(cpu.pc == UINT16_C(0x9000) && cpu.pbr == 0U);
    CHECK(cpu.irq_level);
    return 0;
}

int main(void)
{
    CHECK(test_event_order_and_horizon() == 0);
    CHECK(test_same_time_insertion_budget_cancel_and_capacity() == 0);
    CHECK(test_clock_ratios() == 0);
    CHECK(test_deterministic_replay() == 0);
    CHECK(test_5a22_access_timing() == 0);
    CHECK(test_scheduled_cpu_interrupt_boundary() == 0);
    (void)puts("PASS: deterministic scheduler and 5A22 timing foundation");
    return 0;
}
