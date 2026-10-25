/* MIPSNES2 S-SMP timer block. See LICENSE for GPL-3.0 terms. */
#include "core/apu/spc700/timers.h"

#include <string.h>

static void advance_stage(mipsnes2_smp_timers *timers, unsigned timer,
                          uint64_t ticks)
{
    uint16_t distance;
    uint16_t period;
    uint64_t output_ticks;

    if ((timers->enabled_mask & (uint8_t)(1U << timer)) == 0U || ticks == 0U) {
        return;
    }
    distance = (uint16_t)((uint8_t)(timers->target[timer] -
                                    timers->stage2[timer]));
    if (distance == 0U) {
        distance = 256U;
    }
    if (ticks < distance) {
        timers->stage2[timer] =
            (uint8_t)(timers->stage2[timer] + (uint8_t)ticks);
        return;
    }

    ticks -= distance;
    period = timers->target[timer] == 0U
        ? 256U : timers->target[timer];
    output_ticks = UINT64_C(1) + ticks / period;
    timers->output[timer] = (uint8_t)(
        (timers->output[timer] + (uint8_t)(output_ticks & UINT64_C(0x0f))) &
        UINT8_C(0x0f));
    timers->stage2[timer] = (uint8_t)(ticks % period);
}

void mipsnes2_smp_timers_init(mipsnes2_smp_timers *timers)
{
    if (timers != NULL) {
        (void)memset(timers, 0, sizeof(*timers));
        timers->initialized = true;
    }
}

bool mipsnes2_smp_timers_set_enabled(mipsnes2_smp_timers *timers,
                                     uint8_t enabled_mask)
{
    uint8_t rising;
    unsigned timer;

    if (timers == NULL || !timers->initialized ||
        (enabled_mask & UINT8_C(0xf8)) != 0U) {
        return false;
    }
    rising = (uint8_t)(enabled_mask & (uint8_t)~timers->enabled_mask);
    for (timer = 0U; timer < MIPSNES2_SMP_TIMER_COUNT; ++timer) {
        if ((rising & (uint8_t)(1U << timer)) != 0U) {
            timers->stage2[timer] = 0U;
            timers->output[timer] = 0U;
        }
    }
    timers->enabled_mask = enabled_mask;
    return true;
}

bool mipsnes2_smp_timers_write_target(mipsnes2_smp_timers *timers,
                                      unsigned timer, uint8_t value)
{
    if (timers == NULL || !timers->initialized ||
        timer >= MIPSNES2_SMP_TIMER_COUNT) {
        return false;
    }
    timers->target[timer] = value;
    return true;
}

bool mipsnes2_smp_timers_read_output(mipsnes2_smp_timers *timers,
                                     unsigned timer, uint8_t *value)
{
    if (timers == NULL || !timers->initialized || value == NULL ||
        timer >= MIPSNES2_SMP_TIMER_COUNT) {
        return false;
    }
    *value = timers->output[timer] & UINT8_C(0x0f);
    timers->output[timer] = 0U;
    return true;
}

bool mipsnes2_smp_timers_advance(mipsnes2_smp_timers *timers,
                                 uint64_t smp_cycles)
{
    uint64_t ticks_16;
    uint64_t ticks_128;
    uint8_t remainder_16;
    uint8_t remainder_128;
    uint16_t phase_sum;

    if (timers == NULL || !timers->initialized) {
        return false;
    }
    remainder_16 = (uint8_t)(smp_cycles % UINT64_C(16));
    remainder_128 = (uint8_t)(smp_cycles % UINT64_C(128));
    ticks_16 = smp_cycles / UINT64_C(16);
    if ((uint16_t)((uint16_t)(timers->stage1_phase & UINT8_C(0x0f)) +
                   remainder_16) >= UINT16_C(16)) {
        ++ticks_16;
    }
    ticks_128 = smp_cycles / UINT64_C(128);
    phase_sum = (uint16_t)timers->stage1_phase + remainder_128;
    if (phase_sum >= 128U) {
        ++ticks_128;
    }
    timers->stage1_phase = (uint8_t)(phase_sum & UINT16_C(0x007f));

    advance_stage(timers, 0U, ticks_128);
    advance_stage(timers, 1U, ticks_128);
    advance_stage(timers, 2U, ticks_16);
    return true;
}
