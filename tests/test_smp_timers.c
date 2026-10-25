/* Synthetic S-SMP timer fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/spc700/timers.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static int same_state(const mipsnes2_smp_timers *left,
                      const mipsnes2_smp_timers *right)
{
    return left->initialized == right->initialized &&
           left->enabled_mask == right->enabled_mask &&
           left->stage1_phase == right->stage1_phase &&
           memcmp(left->target, right->target, sizeof(left->target)) == 0 &&
           memcmp(left->stage2, right->stage2, sizeof(left->stage2)) == 0 &&
           memcmp(left->output, right->output, sizeof(left->output)) == 0;
}

int main(void)
{
    mipsnes2_smp_timers timers;
    mipsnes2_smp_timers one_shot;
    mipsnes2_smp_timers chunked;
    uint8_t value = UINT8_C(0xee);
    unsigned i;

    mipsnes2_smp_timers_init(&timers);
    CHECK(timers.initialized && timers.enabled_mask == 0U &&
          timers.stage1_phase == 0U);
    CHECK(!mipsnes2_smp_timers_set_enabled(NULL, 0U));
    CHECK(!mipsnes2_smp_timers_set_enabled(&timers, UINT8_C(0x08)));
    CHECK(!mipsnes2_smp_timers_write_target(&timers, 3U, 1U));
    CHECK(!mipsnes2_smp_timers_read_output(&timers, 3U, &value));
    CHECK(!mipsnes2_smp_timers_read_output(&timers, 0U, NULL));
    CHECK(!mipsnes2_smp_timers_advance(NULL, 1U));

    /* Timer 2 ticks every 16 cycles; timers 0/1 share every eighth edge. */
    CHECK(mipsnes2_smp_timers_write_target(&timers, 0U, 1U));
    CHECK(mipsnes2_smp_timers_write_target(&timers, 1U, 2U));
    CHECK(mipsnes2_smp_timers_write_target(&timers, 2U, 3U));
    CHECK(mipsnes2_smp_timers_set_enabled(&timers, UINT8_C(0x07)));
    CHECK(mipsnes2_smp_timers_advance(&timers, 15U));
    CHECK(timers.output[2] == 0U && timers.stage1_phase == 15U);
    CHECK(mipsnes2_smp_timers_advance(&timers, 1U));
    CHECK(timers.stage2[2] == 1U && timers.output[2] == 0U);
    CHECK(mipsnes2_smp_timers_advance(&timers, 32U));
    CHECK(timers.stage2[2] == 0U && timers.output[2] == 1U);
    CHECK(mipsnes2_smp_timers_advance(&timers, 80U));
    CHECK(timers.output[0] == 1U && timers.output[1] == 0U &&
          timers.stage2[1] == 1U);

    /* Reading clears stage 3 only. */
    CHECK(mipsnes2_smp_timers_read_output(&timers, 2U, &value));
    CHECK(value == 2U && timers.output[2] == 0U &&
          timers.stage2[2] == 2U);

    /* Stage 1 continues while disabled; a rising enable clears stages 2/3
     * but keeps the next shared edge eight cycles away. */
    CHECK(mipsnes2_smp_timers_set_enabled(&timers, 0U));
    CHECK(mipsnes2_smp_timers_advance(&timers, 120U));
    CHECK(timers.stage1_phase == 120U);
    timers.stage2[0] = UINT8_C(9);
    timers.output[0] = UINT8_C(7);
    CHECK(mipsnes2_smp_timers_set_enabled(&timers, UINT8_C(0x01)));
    CHECK(timers.stage2[0] == 0U && timers.output[0] == 0U &&
          timers.stage1_phase == 120U);
    CHECK(mipsnes2_smp_timers_advance(&timers, 7U));
    CHECK(timers.output[0] == 0U);
    CHECK(mipsnes2_smp_timers_advance(&timers, 1U));
    CHECK(timers.output[0] == 1U);

    /* Target zero means 256 stage-1 ticks, and stage 3 wraps at four bits. */
    mipsnes2_smp_timers_init(&timers);
    CHECK(mipsnes2_smp_timers_write_target(&timers, 2U, 0U));
    CHECK(mipsnes2_smp_timers_set_enabled(&timers, UINT8_C(0x04)));
    CHECK(mipsnes2_smp_timers_advance(&timers, UINT64_C(16) * 256U));
    CHECK(timers.output[2] == 1U && timers.stage2[2] == 0U);
    CHECK(mipsnes2_smp_timers_advance(&timers,
                                      UINT64_C(16) * 256U * 15U));
    CHECK(timers.output[2] == 0U);

    /* A target change behind the current stage-2 value waits for wrap. */
    mipsnes2_smp_timers_init(&timers);
    CHECK(mipsnes2_smp_timers_write_target(&timers, 2U, 4U));
    CHECK(mipsnes2_smp_timers_set_enabled(&timers, UINT8_C(0x04)));
    CHECK(mipsnes2_smp_timers_advance(&timers, 32U));
    CHECK(timers.stage2[2] == 2U);
    CHECK(mipsnes2_smp_timers_write_target(&timers, 2U, 1U));
    CHECK(mipsnes2_smp_timers_advance(&timers, UINT64_C(16) * 254U));
    CHECK(timers.output[2] == 0U && timers.stage2[2] == 0U);
    CHECK(mipsnes2_smp_timers_advance(&timers, 16U));
    CHECK(timers.output[2] == 1U && timers.stage2[2] == 0U);

    /* One-shot and irregular chunks must produce identical phase/state. */
    mipsnes2_smp_timers_init(&one_shot);
    mipsnes2_smp_timers_init(&chunked);
    for (i = 0U; i < MIPSNES2_SMP_TIMER_COUNT; ++i) {
        CHECK(mipsnes2_smp_timers_write_target(
            &one_shot, i, (uint8_t)(UINT8_C(3) + i)));
        CHECK(mipsnes2_smp_timers_write_target(
            &chunked, i, (uint8_t)(UINT8_C(3) + i)));
    }
    CHECK(mipsnes2_smp_timers_set_enabled(&one_shot, UINT8_C(0x07)));
    CHECK(mipsnes2_smp_timers_set_enabled(&chunked, UINT8_C(0x07)));
    CHECK(mipsnes2_smp_timers_advance(&one_shot, UINT64_C(1234567)));
    for (i = 0U; i < 17636U; ++i) {
        CHECK(mipsnes2_smp_timers_advance(&chunked, 70U));
    }
    CHECK(mipsnes2_smp_timers_advance(&chunked, 47U));
    CHECK(same_state(&one_shot, &chunked));

    mipsnes2_smp_timers_init(&one_shot);
    mipsnes2_smp_timers_init(&chunked);
    CHECK(mipsnes2_smp_timers_write_target(&one_shot, 0U, 1U));
    CHECK(mipsnes2_smp_timers_write_target(&chunked, 0U, 1U));
    CHECK(mipsnes2_smp_timers_set_enabled(&one_shot, UINT8_C(0x01)));
    CHECK(mipsnes2_smp_timers_set_enabled(&chunked, UINT8_C(0x01)));
    CHECK(mipsnes2_smp_timers_advance(&one_shot, UINT64_MAX));
    CHECK(mipsnes2_smp_timers_advance(
        &chunked, UINT64_MAX - UINT64_C(37)));
    CHECK(mipsnes2_smp_timers_advance(&chunked, UINT64_C(37)));
    CHECK(same_state(&one_shot, &chunked));

    (void)puts("S-SMP timers: OK");
    return 0;
}
