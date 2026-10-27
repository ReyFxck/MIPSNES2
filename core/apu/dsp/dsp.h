/* MIPSNES2 S-DSP register/timing foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_DSP_H
#define MIPSNES2_DSP_H

#include <stdbool.h>
#include <stdint.h>

enum {
    MIPSNES2_DSP_REGISTER_COUNT = 128,
    MIPSNES2_DSP_SAMPLE_CYCLES = 32,
    MIPSNES2_DSP_REGISTER_FLG = 0x6c,
    MIPSNES2_DSP_REGISTER_ENDX = 0x7c
};

/* Register storage is intentionally public for deterministic state inspection.
 * It is not yet a voice, BRR, envelope, echo or PCM implementation. */
typedef struct {
    uint8_t registers[MIPSNES2_DSP_REGISTER_COUNT];
    uint8_t address;
    uint8_t internal_flags;
    uint8_t sample_phase;
    uint64_t smp_cycles;
    uint64_t sample_count;
    bool initialized;
} mipsnes2_dsp;

void mipsnes2_dsp_init(mipsnes2_dsp *dsp);
bool mipsnes2_dsp_write_address(mipsnes2_dsp *dsp, uint8_t address);
bool mipsnes2_dsp_read_address(const mipsnes2_dsp *dsp, uint8_t *address);
bool mipsnes2_dsp_write_data(mipsnes2_dsp *dsp, uint8_t value);
bool mipsnes2_dsp_read_data(const mipsnes2_dsp *dsp, uint8_t *value);

/* Advance the S-DSP's fixed one-sample-per-32-S-SMP-cycle cadence. */
bool mipsnes2_dsp_advance(mipsnes2_dsp *dsp, uint64_t smp_cycles);

#endif
