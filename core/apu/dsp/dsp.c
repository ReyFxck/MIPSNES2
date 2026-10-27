/* MIPSNES2 S-DSP register/timing foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/dsp/dsp.h"

#include <limits.h>
#include <string.h>

static int32_t arithmetic_shift_right(int32_t value, unsigned int amount)
{
    uint32_t magnitude;
    uint32_t bias;

    if (amount == 0U || value >= 0) {
        return value >> amount;
    }
    magnitude = (uint32_t)(-(value + 1)) + UINT32_C(1);
    bias = (UINT32_C(1) << amount) - UINT32_C(1);
    return -(int32_t)((magnitude + bias) >> amount);
}

static int16_t clamp_and_wrap_brr(int32_t value)
{
    uint16_t wrapped;

    if (value > INT16_MAX) {
        value = INT16_MAX;
    } else if (value < INT16_MIN) {
        value = INT16_MIN;
    }
    wrapped = (uint16_t)value & UINT16_C(0x7fff);
    if ((wrapped & UINT16_C(0x4000)) != 0U) {
        return (int16_t)((int32_t)wrapped - INT32_C(0x8000));
    }
    return (int16_t)wrapped;
}

static uint16_t read_ram16(const mipsnes2_dsp *dsp, uint16_t address)
{
    uint16_t next = (uint16_t)(address + UINT16_C(1));

    return (uint16_t)((uint16_t)dsp->ram[address] |
                      ((uint16_t)dsp->ram[next] << 8U));
}

static void begin_key_on(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    uint16_t directory;

    voice->source_number = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_SRCN];
    directory = (uint16_t)(((uint16_t)dsp->registers[
                                MIPSNES2_DSP_REGISTER_DIR] << 8U) +
                           ((uint16_t)voice->source_number * UINT16_C(4)));
    voice->directory_address = directory;
    voice->directory_loaded = dsp->ram != NULL &&
                              dsp->ram_size >= MIPSNES2_DSP_ARAM_SIZE;
    if (voice->directory_loaded) {
        voice->source_start_address = read_ram16(dsp, directory);
        voice->source_loop_address = read_ram16(
            dsp, (uint16_t)(directory + UINT16_C(2)));
    } else {
        voice->source_start_address = 0U;
        voice->source_loop_address = 0U;
    }
    voice->key_on_delay = MIPSNES2_DSP_KEY_ON_DELAY_SAMPLES;
    voice->state = MIPSNES2_DSP_VOICE_KEY_ON_DELAY;
    dsp->registers[MIPSNES2_DSP_REGISTER_ENDX] &=
        (uint8_t)~(UINT8_C(1) << index);
    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_ENVX] = 0U;
    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_OUTX] = 0U;
}

static bool event_work_pending(const mipsnes2_dsp *dsp)
{
    unsigned int index;

    if (dsp->pending_key_on != 0U) {
        return true;
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        const mipsnes2_dsp_voice *voice = &dsp->voices[index];
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);

        if (voice->state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY ||
            ((dsp->internal_flags & UINT8_C(0x80)) != 0U &&
             voice->state != MIPSNES2_DSP_VOICE_WAITING) ||
            ((dsp->registers[MIPSNES2_DSP_REGISTER_KOFF] & bit) != 0U &&
             voice->state != MIPSNES2_DSP_VOICE_WAITING &&
             voice->state != MIPSNES2_DSP_VOICE_RELEASE)) {
            return true;
        }
    }
    return false;
}

static void service_sample_event(mipsnes2_dsp *dsp)
{
    unsigned int index;
    bool poll_keys = (dsp->sample_count & UINT64_C(1)) == 0U;

    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        mipsnes2_dsp_voice *voice = &dsp->voices[index];

        if (voice->state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY) {
            if (voice->key_on_delay != 0U) {
                --voice->key_on_delay;
            }
            if (voice->key_on_delay == 0U) {
                voice->state = MIPSNES2_DSP_VOICE_ACTIVE;
            }
        }
        if ((dsp->internal_flags & UINT8_C(0x80)) != 0U &&
            voice->state != MIPSNES2_DSP_VOICE_WAITING) {
            voice->state = MIPSNES2_DSP_VOICE_WAITING;
            voice->key_on_delay = 0U;
        }
    }
    if (!poll_keys) {
        return;
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);

        if ((dsp->registers[MIPSNES2_DSP_REGISTER_KOFF] & bit) != 0U &&
            dsp->voices[index].state != MIPSNES2_DSP_VOICE_WAITING) {
            dsp->voices[index].state = MIPSNES2_DSP_VOICE_RELEASE;
            dsp->voices[index].key_on_delay = 0U;
        }
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        if ((dsp->pending_key_on & (UINT8_C(1) << index)) != 0U) {
            begin_key_on(dsp, index);
        }
    }
    dsp->pending_key_on = 0U;
}

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

bool mipsnes2_dsp_attach_ram(mipsnes2_dsp *dsp, uint8_t *ram,
                             size_t ram_size)
{
    if (dsp == NULL || !dsp->initialized || ram == NULL ||
        ram_size < MIPSNES2_DSP_ARAM_SIZE) {
        return false;
    }
    dsp->ram = ram;
    dsp->ram_size = ram_size;
    return true;
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
    if (reg == MIPSNES2_DSP_REGISTER_KON) {
        /* KON is sampled at 16 kHz. Multiple writes before a poll collapse to
         * the most recently written mask. */
        dsp->pending_key_on = value;
    } else if (reg == MIPSNES2_DSP_REGISTER_FLG) {
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

bool mipsnes2_dsp_brr_decode_block(const uint8_t *encoded,
                                   size_t encoded_size,
                                   int16_t previous1, int16_t previous2,
                                   mipsnes2_dsp_brr_block *decoded)
{
    mipsnes2_dsp_brr_block result;
    int32_t history1 = previous1;
    int32_t history2 = previous2;
    unsigned int index;

    if (encoded == NULL || decoded == NULL ||
        encoded_size < MIPSNES2_DSP_BRR_BLOCK_SIZE) {
        return false;
    }
    (void)memset(&result, 0, sizeof(result));
    result.header = encoded[0];
    result.shift = encoded[0] >> 4U;
    result.filter = (encoded[0] >> 2U) & UINT8_C(3);
    result.loop = (encoded[0] & UINT8_C(2)) != 0U;
    result.end = (encoded[0] & UINT8_C(1)) != 0U;
    for (index = 0U; index < MIPSNES2_DSP_BRR_SAMPLE_COUNT; ++index) {
        uint8_t packed = encoded[1U + (index / 2U)];
        uint8_t raw = (index & 1U) == 0U ? packed >> 4U
                                         : packed & UINT8_C(0x0f);
        int32_t nibble = raw < UINT8_C(8) ? (int32_t)raw
                                          : (int32_t)raw - INT32_C(16);
        int32_t sample;

        if (result.shift <= 12U) {
            sample = arithmetic_shift_right(
                nibble * (INT32_C(1) << result.shift), 1U);
        } else {
            sample = nibble < 0 ? INT32_C(-2048) : 0;
        }
        if (result.filter == 1U) {
            sample += history1 + arithmetic_shift_right(-history1, 4U);
        } else if (result.filter == 2U) {
            sample += (INT32_C(2) * history1) +
                      arithmetic_shift_right(INT32_C(-3) * history1, 5U) -
                      history2 + arithmetic_shift_right(history2, 4U);
        } else if (result.filter == 3U) {
            sample += (INT32_C(2) * history1) +
                      arithmetic_shift_right(INT32_C(-13) * history1, 6U) -
                      history2 + arithmetic_shift_right(
                          INT32_C(3) * history2, 4U);
        }
        history2 = history1;
        history1 = clamp_and_wrap_brr(sample);
        result.samples[index] = (int16_t)history1;
    }
    result.previous1 = (int16_t)history1;
    result.previous2 = (int16_t)history2;
    *decoded = result;
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
    while (samples != 0U && event_work_pending(dsp)) {
        service_sample_event(dsp);
        ++dsp->sample_count;
        --samples;
    }
    dsp->sample_count += samples;
    return true;
}
