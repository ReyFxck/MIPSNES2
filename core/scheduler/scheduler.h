/* MIPSNES2 deterministic scheduler foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_SCHEDULER_H
#define MIPSNES2_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t mipsnes2_master_clock;
typedef uint64_t mipsnes2_scheduler_token;
typedef uint32_t mipsnes2_scheduler_priority;

enum {
    MIPSNES2_SCHEDULER_EVENT_CAPACITY = 64
};

typedef enum {
    MIPSNES2_SCHEDULER_OK = 0,
    MIPSNES2_SCHEDULER_INVALID_ARGUMENT,
    MIPSNES2_SCHEDULER_CAPACITY_EXHAUSTED,
    MIPSNES2_SCHEDULER_TIME_REGRESSION,
    MIPSNES2_SCHEDULER_TIME_OVERFLOW,
    MIPSNES2_SCHEDULER_TOKEN_EXHAUSTED,
    MIPSNES2_SCHEDULER_EVENT_NOT_FOUND,
    MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED,
    MIPSNES2_SCHEDULER_REENTRANT_RUN
} mipsnes2_scheduler_result;

typedef void (*mipsnes2_scheduler_callback)(void *context,
                                             mipsnes2_master_clock timestamp,
                                             mipsnes2_scheduler_token token);

typedef struct {
    mipsnes2_master_clock timestamp;
    mipsnes2_scheduler_priority priority;
    mipsnes2_scheduler_token token;
    mipsnes2_scheduler_callback callback;
    void *context;
} mipsnes2_scheduler_event;

typedef struct {
    mipsnes2_master_clock now;
    mipsnes2_scheduler_token next_token;
    size_t event_count;
    bool dispatching;
    mipsnes2_scheduler_event events[MIPSNES2_SCHEDULER_EVENT_CAPACITY];
} mipsnes2_scheduler;

typedef struct {
    uint32_t numerator;
    uint32_t denominator;
    uint64_t remainder;
} mipsnes2_clock_ratio;

void mipsnes2_scheduler_init(mipsnes2_scheduler *scheduler);
mipsnes2_scheduler_result mipsnes2_scheduler_schedule(
    mipsnes2_scheduler *scheduler,
    mipsnes2_master_clock timestamp,
    mipsnes2_scheduler_priority priority,
    mipsnes2_scheduler_callback callback,
    void *context,
    mipsnes2_scheduler_token *token);
mipsnes2_scheduler_result mipsnes2_scheduler_cancel(
    mipsnes2_scheduler *scheduler,
    mipsnes2_scheduler_token token);
bool mipsnes2_scheduler_next_event_time(const mipsnes2_scheduler *scheduler,
                                        mipsnes2_master_clock *timestamp);
mipsnes2_master_clock mipsnes2_scheduler_event_horizon(
    const mipsnes2_scheduler *scheduler,
    mipsnes2_master_clock requested_horizon);
mipsnes2_scheduler_result mipsnes2_scheduler_run_until(
    mipsnes2_scheduler *scheduler,
    mipsnes2_master_clock target,
    size_t event_budget,
    size_t *events_dispatched);

mipsnes2_scheduler_result mipsnes2_clock_ratio_init(
    mipsnes2_clock_ratio *ratio,
    uint32_t numerator,
    uint32_t denominator);
mipsnes2_scheduler_result mipsnes2_clock_ratio_advance(
    mipsnes2_clock_ratio *ratio,
    uint64_t source_ticks,
    mipsnes2_master_clock *master_clocks);

#endif
