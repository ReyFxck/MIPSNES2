/* MIPSNES2 deterministic scheduler foundation. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/scheduler.h"

#include <limits.h>
#include <string.h>

static bool event_precedes(const mipsnes2_scheduler_event *left,
                           const mipsnes2_scheduler_event *right)
{
    if (left->timestamp != right->timestamp) {
        return left->timestamp < right->timestamp;
    }
    if (left->priority != right->priority) {
        return left->priority < right->priority;
    }
    return left->token < right->token;
}

static size_t earliest_event_index(const mipsnes2_scheduler *scheduler)
{
    size_t index;
    size_t earliest = 0U;

    for (index = 1U; index < scheduler->event_count; ++index) {
        if (event_precedes(&scheduler->events[index],
                           &scheduler->events[earliest])) {
            earliest = index;
        }
    }
    return earliest;
}

static void remove_event(mipsnes2_scheduler *scheduler, size_t index)
{
    size_t remaining = scheduler->event_count - index - 1U;

    if (remaining != 0U) {
        (void)memmove(&scheduler->events[index],
                      &scheduler->events[index + 1U],
                      remaining * sizeof(scheduler->events[0]));
    }
    scheduler->event_count--;
}

static uint32_t greatest_common_divisor(uint32_t left, uint32_t right)
{
    while (right != 0U) {
        uint32_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

void mipsnes2_scheduler_init(mipsnes2_scheduler *scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    (void)memset(scheduler, 0, sizeof(*scheduler));
    scheduler->next_token = UINT64_C(1);
}

mipsnes2_scheduler_result mipsnes2_scheduler_schedule(
    mipsnes2_scheduler *scheduler,
    mipsnes2_master_clock timestamp,
    mipsnes2_scheduler_priority priority,
    mipsnes2_scheduler_callback callback,
    void *context,
    mipsnes2_scheduler_token *token)
{
    mipsnes2_scheduler_event *event;

    if (scheduler == NULL || callback == NULL) {
        return MIPSNES2_SCHEDULER_INVALID_ARGUMENT;
    }
    if (timestamp < scheduler->now) {
        return MIPSNES2_SCHEDULER_TIME_REGRESSION;
    }
    if (scheduler->event_count == MIPSNES2_SCHEDULER_EVENT_CAPACITY) {
        return MIPSNES2_SCHEDULER_CAPACITY_EXHAUSTED;
    }
    if (scheduler->next_token == 0U) {
        return MIPSNES2_SCHEDULER_TOKEN_EXHAUSTED;
    }

    event = &scheduler->events[scheduler->event_count++];
    event->timestamp = timestamp;
    event->priority = priority;
    event->token = scheduler->next_token;
    event->callback = callback;
    event->context = context;
    if (token != NULL) {
        *token = event->token;
    }
    scheduler->next_token++;
    return MIPSNES2_SCHEDULER_OK;
}

mipsnes2_scheduler_result mipsnes2_scheduler_cancel(
    mipsnes2_scheduler *scheduler,
    mipsnes2_scheduler_token token)
{
    size_t index;

    if (scheduler == NULL || token == 0U) {
        return MIPSNES2_SCHEDULER_INVALID_ARGUMENT;
    }
    for (index = 0U; index < scheduler->event_count; ++index) {
        if (scheduler->events[index].token == token) {
            remove_event(scheduler, index);
            return MIPSNES2_SCHEDULER_OK;
        }
    }
    return MIPSNES2_SCHEDULER_EVENT_NOT_FOUND;
}

bool mipsnes2_scheduler_next_event_time(const mipsnes2_scheduler *scheduler,
                                        mipsnes2_master_clock *timestamp)
{
    size_t index;

    if (scheduler == NULL || timestamp == NULL || scheduler->event_count == 0U) {
        return false;
    }
    index = earliest_event_index(scheduler);
    *timestamp = scheduler->events[index].timestamp;
    return true;
}

mipsnes2_master_clock mipsnes2_scheduler_event_horizon(
    const mipsnes2_scheduler *scheduler,
    mipsnes2_master_clock requested_horizon)
{
    mipsnes2_master_clock next;

    if (mipsnes2_scheduler_next_event_time(scheduler, &next) &&
        next < requested_horizon) {
        return next;
    }
    return requested_horizon;
}

mipsnes2_scheduler_result mipsnes2_scheduler_run_until(
    mipsnes2_scheduler *scheduler,
    mipsnes2_master_clock target,
    size_t event_budget,
    size_t *events_dispatched)
{
    size_t dispatched = 0U;

    if (events_dispatched != NULL) {
        *events_dispatched = 0U;
    }
    if (scheduler == NULL) {
        return MIPSNES2_SCHEDULER_INVALID_ARGUMENT;
    }
    if (target < scheduler->now) {
        return MIPSNES2_SCHEDULER_TIME_REGRESSION;
    }
    if (scheduler->dispatching) {
        return MIPSNES2_SCHEDULER_REENTRANT_RUN;
    }

    scheduler->dispatching = true;
    while (scheduler->event_count != 0U) {
        size_t index = earliest_event_index(scheduler);
        mipsnes2_scheduler_event event = scheduler->events[index];

        if (event.timestamp > target) {
            break;
        }
        if (dispatched == event_budget) {
            scheduler->dispatching = false;
            if (events_dispatched != NULL) {
                *events_dispatched = dispatched;
            }
            return MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED;
        }

        remove_event(scheduler, index);
        scheduler->now = event.timestamp;
        event.callback(event.context, event.timestamp, event.token);
        dispatched++;
    }
    scheduler->now = target;
    scheduler->dispatching = false;
    if (events_dispatched != NULL) {
        *events_dispatched = dispatched;
    }
    return MIPSNES2_SCHEDULER_OK;
}

mipsnes2_scheduler_result mipsnes2_clock_ratio_init(
    mipsnes2_clock_ratio *ratio,
    uint32_t numerator,
    uint32_t denominator)
{
    uint32_t divisor;

    if (ratio == NULL || numerator == 0U || denominator == 0U) {
        return MIPSNES2_SCHEDULER_INVALID_ARGUMENT;
    }
    divisor = greatest_common_divisor(numerator, denominator);
    ratio->numerator = numerator / divisor;
    ratio->denominator = denominator / divisor;
    ratio->remainder = 0U;
    return MIPSNES2_SCHEDULER_OK;
}

mipsnes2_scheduler_result mipsnes2_clock_ratio_advance(
    mipsnes2_clock_ratio *ratio,
    uint64_t source_ticks,
    mipsnes2_master_clock *master_clocks)
{
    uint64_t whole;
    uint64_t partial;
    uint64_t carry;
    uint64_t converted;

    if (ratio == NULL || master_clocks == NULL || ratio->numerator == 0U ||
        ratio->denominator == 0U || ratio->remainder >= ratio->denominator) {
        return MIPSNES2_SCHEDULER_INVALID_ARGUMENT;
    }

    whole = source_ticks / ratio->denominator;
    partial = (source_ticks % ratio->denominator) * ratio->numerator +
              ratio->remainder;
    carry = partial / ratio->denominator;
    if (whole > (UINT64_MAX - carry) / ratio->numerator) {
        return MIPSNES2_SCHEDULER_TIME_OVERFLOW;
    }
    converted = whole * ratio->numerator + carry;
    ratio->remainder = partial % ratio->denominator;
    *master_clocks = converted;
    return MIPSNES2_SCHEDULER_OK;
}
