/* MIPSNES2 S-SMP timer block. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_SMP_TIMERS_H
#define MIPSNES2_SMP_TIMERS_H

#include <stdbool.h>
#include <stdint.h>

enum { MIPSNES2_SMP_TIMER_COUNT = 3 };

/* Caller-owned timer state. stage1_phase is the shared free-running 128-cycle
 * prescaler phase; timer 2 also observes each 16-cycle subdivision. */
typedef struct {
    uint8_t target[MIPSNES2_SMP_TIMER_COUNT];
    uint8_t stage2[MIPSNES2_SMP_TIMER_COUNT];
    uint8_t output[MIPSNES2_SMP_TIMER_COUNT];
    uint8_t enabled_mask;
    uint8_t stage1_phase;
    bool initialized;
} mipsnes2_smp_timers;

/* Establishes deterministic zero state. The real power-on target/output values
 * and DSP-relative prescaler phase are deliberately not claimed. */
void mipsnes2_smp_timers_init(mipsnes2_smp_timers *timers);

/* $F1 bits 0-2. A 0->1 transition clears only that timer's stages 2 and 3;
 * the shared stage-1 phase continues. */
bool mipsnes2_smp_timers_set_enabled(mipsnes2_smp_timers *timers,
                                     uint8_t enabled_mask);

/* $FA-$FC are write-only targets. Zero represents a 256-tick period. */
bool mipsnes2_smp_timers_write_target(mipsnes2_smp_timers *timers,
                                      unsigned timer, uint8_t value);

/* $FD-$FF return the low four-bit stage-3 count and clear that count. */
bool mipsnes2_smp_timers_read_output(mipsnes2_smp_timers *timers,
                                     unsigned timer, uint8_t *value);

/* Advance by S-SMP clock cycles. Stage 1 is free-running while timers are
 * disabled, so later enable transitions retain the common prescaler phase. */
bool mipsnes2_smp_timers_advance(mipsnes2_smp_timers *timers,
                                 uint64_t smp_cycles);

#endif
