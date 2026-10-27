/* MIPSNES2 S-DSP register/timing foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/dsp/dsp.h"

#include <string.h>

void mipsnes2_dsp_init(mipsnes2_dsp *dsp)
{
    if (dsp != NULL) {
        (void)memset(dsp, 0, sizeof(*dsp));
        /* The internal reset/mute/echo-disable state is distinct from the
         * readable register shadow, whose electrical reset value is not
         * claimed by this foundation. */
        dsp->internal_flags = UINT8_C(0xe0);
        dsp->initialized = true;
    }
}

bool mipsnes2_dsp_write_address(mipsnes2_dsp *dsp, uint8_t address)
{
    if (dsp == NULL || !dsp->initialized) {
        return false;
    }
    dsp->address = address;
    return true;
}

bool mipsnes2_dsp_read_address(const mipsnes2_dsp *dsp, uint8_t *address)
{
    if (dsp == NULL || !dsp->initialized || address == NULL) {
        return false;
    }
    *address = dsp->address;
    return true;
}

bool mipsnes2_dsp_write_data(mipsnes2_dsp *dsp, uint8_t value)
{
    uint8_t reg;

    if (dsp == NULL || !dsp->initialized) {
        return false;
    }
    /* Address bit 7 inhibits S-DSP writes but does not change the latch. */
    if ((dsp->address & UINT8_C(0x80)) != 0U) {
        return true;
    }
    reg = dsp->address & UINT8_C(0x7f);
    if (reg == MIPSNES2_DSP_REGISTER_ENDX) {
        /* Any CPU write acknowledges and clears all voice-end bits. */
        dsp->registers[reg] = 0U;
        return true;
    }
    dsp->registers[reg] = value;
    if (reg == MIPSNES2_DSP_REGISTER_FLG) {
        dsp->internal_flags = value;
    }
    return true;
}

bool mipsnes2_dsp_read_data(const mipsnes2_dsp *dsp, uint8_t *value)
{
    if (dsp == NULL || !dsp->initialized || value == NULL) {
        return false;
    }
    *value = dsp->registers[dsp->address & UINT8_C(0x7f)];
    return true;
}

bool mipsnes2_dsp_advance(mipsnes2_dsp *dsp, uint64_t smp_cycles)
{
    uint64_t samples;
    uint64_t whole;
    uint8_t remainder;
    uint16_t phase;

    if (dsp == NULL || !dsp->initialized ||
        dsp->smp_cycles > UINT64_MAX - smp_cycles) {
        return false;
    }
    whole = smp_cycles / MIPSNES2_DSP_SAMPLE_CYCLES;
    remainder = (uint8_t)(smp_cycles % MIPSNES2_DSP_SAMPLE_CYCLES);
    phase = (uint16_t)dsp->sample_phase + remainder;
    samples = whole + (phase / MIPSNES2_DSP_SAMPLE_CYCLES);
    if (dsp->sample_count > UINT64_MAX - samples) {
        return false;
    }
    dsp->sample_phase = (uint8_t)(phase % MIPSNES2_DSP_SAMPLE_CYCLES);
    dsp->smp_cycles += smp_cycles;
    dsp->sample_count += samples;
    return true;
}
