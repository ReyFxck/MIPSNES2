/* MIPSNES2 S-DSP register/timing foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/dsp/dsp.h"

#include <limits.h>
#include <string.h>

enum {
    DSP_ENVELOPE_COUNTER_PERIOD = 0x7800,
    DSP_ENVELOPE_MAX = 0x07ff
};

static const uint16_t envelope_counter_rates[32] = {
    0U, 2048U, 1536U, 1280U, 1024U, 768U, 640U, 512U,
    384U, 320U, 256U, 192U, 160U, 128U, 96U, 80U,
    64U, 48U, 40U, 32U, 24U, 20U, 16U, 12U,
    10U, 8U, 6U, 5U, 4U, 3U, 2U, 1U
};

static const uint16_t envelope_counter_offsets[32] = {
    0U, 0U, 1040U, 536U, 0U, 1040U, 536U, 0U,
    1040U, 536U, 0U, 1040U, 536U, 0U, 1040U, 536U,
    0U, 1040U, 536U, 0U, 1040U, 536U, 0U, 1040U,
    536U, 0U, 1040U, 536U, 0U, 1040U, 0U, 0U
};

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

static int16_t clamp_signed16(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static int16_t wrap_signed16(int32_t value)
{
    uint16_t wrapped = (uint16_t)((uint32_t)value & UINT32_C(0xffff));

    return wrapped <= (uint16_t)INT16_MAX
               ? (int16_t)wrapped
               : (int16_t)((int32_t)wrapped - INT32_C(0x10000));
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

static int16_t decode_brr_sample(uint8_t raw, uint8_t shift,
                                 uint8_t filter, int16_t *previous1,
                                 int16_t *previous2)
{
    int32_t history1 = *previous1;
    int32_t history2 = *previous2;
    int32_t nibble = raw < UINT8_C(8) ? (int32_t)raw
                                      : (int32_t)raw - INT32_C(16);
    int32_t sample;
    int16_t result;

    if (shift <= 12U) {
        sample = arithmetic_shift_right(
            nibble * (INT32_C(1) << shift), 1U);
    } else {
        sample = nibble < 0 ? INT32_C(-2048) : 0;
    }
    if (filter == 1U) {
        sample += history1 + arithmetic_shift_right(-history1, 4U);
    } else if (filter == 2U) {
        sample += (INT32_C(2) * history1) +
                  arithmetic_shift_right(INT32_C(-3) * history1, 5U) -
                  history2 + arithmetic_shift_right(history2, 4U);
    } else if (filter == 3U) {
        sample += (INT32_C(2) * history1) +
                  arithmetic_shift_right(INT32_C(-13) * history1, 6U) -
                  history2 + arithmetic_shift_right(
                      INT32_C(3) * history2, 4U);
    }
    result = clamp_and_wrap_brr(sample);
    *previous2 = *previous1;
    *previous1 = result;
    return result;
}

static uint16_t read_ram16(const mipsnes2_dsp *dsp, uint16_t address)
{
    uint16_t next = (uint16_t)(address + UINT16_C(1));

    return (uint16_t)((uint16_t)dsp->ram[address] |
                      ((uint16_t)dsp->ram[next] << 8U));
}

static uint16_t voice_pitch(const mipsnes2_dsp *dsp, unsigned int index)
{
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

    return (uint16_t)(((uint16_t)dsp->registers[
                           base + MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH]
                       << 8U) |
                      dsp->registers[
                          base + MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW]) &
           UINT16_C(0x3fff);
}

static void advance_envelope_counter(mipsnes2_dsp *dsp, uint64_t samples)
{
    uint16_t delta = (uint16_t)(
        samples % (uint64_t)DSP_ENVELOPE_COUNTER_PERIOD);

    dsp->envelope_counter = (uint16_t)(
        (dsp->envelope_counter + DSP_ENVELOPE_COUNTER_PERIOD - delta) %
        DSP_ENVELOPE_COUNTER_PERIOD);
}

static bool envelope_counter_event(const mipsnes2_dsp *dsp, uint8_t rate)
{
    uint16_t period = envelope_counter_rates[rate];

    return period != 0U &&
           ((uint32_t)dsp->envelope_counter +
            envelope_counter_offsets[rate]) % period == 0U;
}

static uint64_t counter_event_count(uint16_t counter, uint64_t samples,
                                    uint8_t rate)
{
    uint16_t period = envelope_counter_rates[rate];
    uint16_t first;

    if (period == 0U || samples == 0U) {
        return 0U;
    }
    first = (uint16_t)(((uint32_t)counter +
                        envelope_counter_offsets[rate]) % period);
    if (first == 0U) {
        first = period;
    }
    if (samples < first) {
        return 0U;
    }
    return UINT64_C(1) +
           (samples - (uint64_t)first) / (uint64_t)period;
}

static uint16_t step_noise_lfsr(uint16_t value)
{
    uint16_t feedback = (uint16_t)(
        ((value ^ (value >> 1U)) & UINT16_C(1)) << 14U);

    return (uint16_t)((value >> 1U) | feedback);
}

static int16_t current_noise_sample(const mipsnes2_dsp *dsp)
{
    uint16_t value = dsp->noise_lfsr;

    return (int16_t)((value & UINT16_C(0x4000)) != 0U
                         ? (int32_t)value - INT32_C(0x8000)
                         : (int32_t)value);
}

static uint16_t apply_noise_transform(const uint16_t transform[15],
                                      uint16_t value)
{
    uint16_t result = 0U;
    unsigned int bit;

    for (bit = 0U; bit < 15U; ++bit) {
        if ((value & (uint16_t)(UINT16_C(1) << bit)) != 0U) {
            result ^= transform[bit];
        }
    }
    return result;
}

static void advance_noise_lfsr(mipsnes2_dsp *dsp, uint64_t events)
{
    uint16_t transform[15];
    uint16_t squared[15];
    uint16_t value = dsp->noise_lfsr;
    unsigned int bit;

    if (events <= UINT64_C(64)) {
        while (events != 0U) {
            value = step_noise_lfsr(value);
            --events;
        }
        dsp->noise_lfsr = value;
        return;
    }
    for (bit = 0U; bit < 15U; ++bit) {
        transform[bit] = step_noise_lfsr(
            (uint16_t)(UINT16_C(1) << bit));
    }
    while (events != 0U) {
        if ((events & UINT64_C(1)) != 0U) {
            value = apply_noise_transform(transform, value);
        }
        for (bit = 0U; bit < 15U; ++bit) {
            squared[bit] = apply_noise_transform(
                transform, transform[bit]);
        }
        (void)memcpy(transform, squared, sizeof(transform));
        events >>= 1U;
    }
    dsp->noise_lfsr = value;
}

static void advance_global_state(mipsnes2_dsp *dsp, uint64_t samples)
{
    uint8_t rate = dsp->internal_flags & UINT8_C(0x1f);
    uint64_t noise_events = counter_event_count(
        dsp->envelope_counter, samples, rate);

    advance_envelope_counter(dsp, samples);
    advance_noise_lfsr(dsp, noise_events);
}

static uint16_t clamp_envelope(int32_t value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > DSP_ENVELOPE_MAX) {
        return DSP_ENVELOPE_MAX;
    }
    return (uint16_t)value;
}

static void update_envelope_register(mipsnes2_dsp *dsp,
                                     unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_ENVX] =
        (uint8_t)(voice->envelope >> 4U);
}

static int32_t signed_register_value(uint8_t value)
{
    return value < UINT8_C(0x80) ? (int32_t)value
                                 : (int32_t)value - INT32_C(0x100);
}

static void update_voice_noise_output(mipsnes2_dsp *dsp,
                                      unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t bit = (uint8_t)(UINT8_C(1) << index);
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    int32_t adjusted;

    if ((dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) == 0U) {
        voice->output_sample_valid = false;
        return;
    }
    voice->source_sample = current_noise_sample(dsp);
    adjusted = arithmetic_shift_right(
        (int32_t)voice->source_sample * (int32_t)voice->envelope, 11U);
    voice->output_sample = (int16_t)adjusted;
    voice->left_sample = (int16_t)arithmetic_shift_right(
        (int32_t)voice->output_sample * signed_register_value(
            dsp->registers[
                base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT]),
        7U);
    voice->right_sample = (int16_t)arithmetic_shift_right(
        (int32_t)voice->output_sample * signed_register_value(
            dsp->registers[
                base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT]),
        7U);
    voice->output_sample_valid = true;
    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_OUTX] =
        (uint8_t)((uint16_t)voice->output_sample >> 8U);
}

static void update_resolved_dry_mix(mipsnes2_dsp *dsp)
{
    int32_t left = 0;
    int32_t right = 0;
    uint8_t mask = 0U;
    unsigned int index;

    /* Saturation is observable, so preserve the documented Voice 0-to-7
     * accumulation order and clamp after every individual addition. */
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        const mipsnes2_dsp_voice *voice = &dsp->voices[index];

        if (!voice->output_sample_valid) {
            continue;
        }
        left = clamp_signed16(left + (int32_t)voice->left_sample);
        right = clamp_signed16(right + (int32_t)voice->right_sample);
        mask |= (uint8_t)(UINT8_C(1) << index);
    }
    dsp->dry_mix.voice_mix.left = (int16_t)left;
    dsp->dry_mix.voice_mix.right = (int16_t)right;
    dsp->dry_mix.main_sample.left = wrap_signed16(arithmetic_shift_right(
        left * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT]),
        7U));
    dsp->dry_mix.main_sample.right = wrap_signed16(arithmetic_shift_right(
        right * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT]),
        7U));
    dsp->dry_mix.resolved_voice_mask = mask;
}

static void refresh_idle_noise_outputs(mipsnes2_dsp *dsp)
{
    unsigned int index;
    int16_t source = current_noise_sample(dsp);

    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        mipsnes2_dsp_voice *voice = &dsp->voices[index];
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);
        uint8_t base = (uint8_t)(
            index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

        if ((dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) != 0U) {
            voice->source_sample = source;
            voice->output_sample = 0;
            voice->left_sample = 0;
            voice->right_sample = 0;
            voice->output_sample_valid = true;
            dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_OUTX] = 0U;
        }
    }
    update_resolved_dry_mix(dsp);
}

static void release_envelope(mipsnes2_dsp *dsp, unsigned int index,
                             bool clear)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];

    voice->envelope_state = MIPSNES2_DSP_ENVELOPE_RELEASE;
    if (clear) {
        voice->envelope = 0U;
        voice->envelope_calculation = 0;
        update_envelope_register(dsp, index);
    }
}

static void update_voice_envelope(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    uint8_t adsr1 = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_ADSR1];
    uint8_t adsr2 = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_ADSR2];
    uint8_t gain = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_GAIN];
    int32_t next = voice->envelope;
    uint8_t rate = 31U;
    bool direct = false;

    if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_RELEASE) {
        next -= INT32_C(8);
    } else if ((adsr1 & UINT8_C(0x80)) == 0U) {
        if ((gain & UINT8_C(0x80)) == 0U) {
            next = (int32_t)(gain & UINT8_C(0x7f)) << 4U;
            direct = true;
        } else {
            uint8_t mode = (gain >> 5U) & UINT8_C(3);

            rate = gain & UINT8_C(0x1f);
            if (mode == 0U) {
                next -= INT32_C(32);
            } else if (mode == 1U) {
                next -= arithmetic_shift_right(next - INT32_C(1), 8U) +
                        INT32_C(1);
            } else if (mode == 2U) {
                next += INT32_C(32);
            } else {
                next += voice->envelope_calculation < INT32_C(0x600)
                            ? INT32_C(32) : INT32_C(8);
            }
        }
    } else if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_ATTACK) {
        uint8_t attack = adsr1 & UINT8_C(0x0f);

        rate = (uint8_t)((attack << 1U) | UINT8_C(1));
        next += attack == UINT8_C(0x0f) ? INT32_C(1024)
                                                : INT32_C(32);
    } else {
        if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_DECAY) {
            rate = (uint8_t)(UINT8_C(16) +
                             (((adsr1 >> 4U) & UINT8_C(7)) << 1U));
        } else {
            rate = adsr2 & UINT8_C(0x1f);
        }
        next -= arithmetic_shift_right(next - INT32_C(1), 8U) +
                INT32_C(1);
    }

    if (direct ||
        voice->envelope_state == MIPSNES2_DSP_ENVELOPE_RELEASE ||
        envelope_counter_event(dsp, rate)) {
        voice->envelope = clamp_envelope(next);
    }
    if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_DECAY &&
        ((uint32_t)next >> 8U) == (uint32_t)(adsr2 >> 5U)) {
        voice->envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
    } else if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_ATTACK &&
               (next < 0 || next > DSP_ENVELOPE_MAX)) {
        voice->envelope_state = MIPSNES2_DSP_ENVELOPE_DECAY;
    }
    voice->envelope_calculation = next;
    update_envelope_register(dsp, index);
}

static void refresh_loop_address(mipsnes2_dsp *dsp, unsigned int index)
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
    voice->source_loop_address = read_ram16(
        dsp, (uint16_t)(directory + UINT16_C(2)));
}

static void decode_voice_brr_group(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t shift;
    uint8_t filter;
    unsigned int byte_index;

    if (!voice->brr_stream_active) {
        return;
    }
    if (voice->brr_group_index == 0U) {
        voice->brr_header = dsp->ram[voice->brr_address];
        if ((voice->brr_header & UINT8_C(3)) == UINT8_C(1)) {
            voice->state = MIPSNES2_DSP_VOICE_RELEASE;
            release_envelope(dsp, index, true);
        }
    }
    shift = voice->brr_header >> 4U;
    filter = (voice->brr_header >> 2U) & UINT8_C(3);
    for (byte_index = 0U; byte_index < 2U; ++byte_index) {
        uint16_t address = (uint16_t)(
            voice->brr_address + UINT16_C(1) +
            ((uint16_t)voice->brr_group_index * UINT16_C(2)) +
            (uint16_t)byte_index);
        uint8_t packed = dsp->ram[address];
        uint8_t destination = (uint8_t)(
            voice->ring_write_offset + (byte_index * 2U));

        voice->brr_ring[destination] = decode_brr_sample(
            packed >> 4U, shift, filter, &voice->brr_previous1,
            &voice->brr_previous2);
        voice->brr_ring[destination + 1U] = decode_brr_sample(
            packed & UINT8_C(0x0f), shift, filter,
            &voice->brr_previous1, &voice->brr_previous2);
    }
    voice->ring_write_offset = (uint8_t)(
        (voice->ring_write_offset + MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT) %
        MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT);
    ++voice->brr_group_index;
    if (voice->decoded_groups != UINT64_MAX) {
        ++voice->decoded_groups;
    }
    if (voice->brr_group_index ==
        MIPSNES2_DSP_BRR_SAMPLE_COUNT /
        MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT) {
        voice->brr_group_index = 0U;
        if ((voice->brr_header & UINT8_C(1)) != 0U) {
            dsp->registers[MIPSNES2_DSP_REGISTER_ENDX] |=
                (uint8_t)(UINT8_C(1) << index);
            refresh_loop_address(dsp, index);
            voice->brr_address = voice->source_loop_address;
        } else {
            voice->brr_address = (uint16_t)(
                voice->brr_address + MIPSNES2_DSP_BRR_BLOCK_SIZE);
        }
    }
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
    voice->brr_address = voice->source_start_address;
    voice->interpolation_index = 0U;
    voice->pitch = voice_pitch(dsp, index);
    voice->decoded_groups = 0U;
    voice->envelope = 0U;
    voice->envelope_calculation = 0;
    voice->output_sample = 0;
    voice->left_sample = 0;
    voice->right_sample = 0;
    voice->output_sample_valid =
        (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
         (uint8_t)(UINT8_C(1) << index)) != 0U;
    voice->source_sample = voice->output_sample_valid
                               ? current_noise_sample(dsp) : 0;
    voice->envelope_state = MIPSNES2_DSP_ENVELOPE_ATTACK;
    voice->brr_group_index = 0U;
    voice->brr_header = 0U;
    voice->ring_offset = 0U;
    voice->ring_write_offset = 0U;
    voice->brr_previous1 = voice->brr_ring[
        MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT - 1U];
    voice->brr_previous2 = voice->brr_ring[
        MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT - 2U];
    voice->brr_stream_active = voice->directory_loaded;
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
        bool noise_selected =
            (dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) != 0U;

        if (voice->key_on_delay != 0U ||
            voice->output_sample_valid != noise_selected ||
            (noise_selected && voice->envelope == 0U &&
             voice->output_sample != 0) ||
            voice->state == MIPSNES2_DSP_VOICE_ACTIVE ||
            (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_RELEASE &&
             voice->envelope != 0U) ||
            (voice->brr_stream_active &&
             (voice->interpolation_index >= UINT16_C(0x4000) ||
              voice_pitch(dsp, index) != 0U)) ||
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

    advance_global_state(dsp, 1U);
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        mipsnes2_dsp_voice *voice = &dsp->voices[index];
        bool update_envelope = false;

        if (voice->key_on_delay != 0U) {
            if (voice->brr_stream_active && voice->key_on_delay <= 4U &&
                voice->key_on_delay >= 2U) {
                decode_voice_brr_group(dsp, index);
            }
            --voice->key_on_delay;
            update_envelope = voice->key_on_delay == 0U;
            if (voice->key_on_delay == 0U &&
                voice->state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY) {
                voice->state = MIPSNES2_DSP_VOICE_ACTIVE;
            }
        } else if (voice->brr_stream_active) {
            uint32_t position;

            if (voice->interpolation_index >= UINT16_C(0x4000)) {
                voice->ring_offset = (uint8_t)(
                    (voice->ring_offset +
                     MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT) %
                    MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT);
                decode_voice_brr_group(dsp, index);
                voice->interpolation_index &= UINT16_C(0x3fff);
            }
            voice->pitch = voice_pitch(dsp, index);
            position = (uint32_t)voice->interpolation_index + voice->pitch;
            if (position > UINT16_C(0x7fff)) {
                position = UINT16_C(0x7fff);
            }
            voice->interpolation_index = (uint16_t)position;
            update_envelope = voice->state == MIPSNES2_DSP_VOICE_ACTIVE ||
                              voice->state == MIPSNES2_DSP_VOICE_RELEASE;
        } else {
            update_envelope = voice->state == MIPSNES2_DSP_VOICE_ACTIVE ||
                              voice->state == MIPSNES2_DSP_VOICE_RELEASE;
        }
        update_voice_noise_output(dsp, index);
        if ((dsp->internal_flags & UINT8_C(0x80)) != 0U &&
            voice->state != MIPSNES2_DSP_VOICE_WAITING) {
            voice->state = MIPSNES2_DSP_VOICE_WAITING;
            release_envelope(dsp, index, true);
            voice->source_sample = 0;
            voice->output_sample = 0;
            voice->left_sample = 0;
            voice->right_sample = 0;
            voice->output_sample_valid =
                (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
                 (uint8_t)(UINT8_C(1) << index)) != 0U;
            dsp->registers[
                (index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE) +
                MIPSNES2_DSP_VOICE_REGISTER_OUTX] = 0U;
        } else if (update_envelope) {
            update_voice_envelope(dsp, index);
        }
    }
    update_resolved_dry_mix(dsp);
    if (!poll_keys) {
        return;
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);

        if ((dsp->registers[MIPSNES2_DSP_REGISTER_KOFF] & bit) != 0U &&
            dsp->voices[index].state != MIPSNES2_DSP_VOICE_WAITING) {
            dsp->voices[index].state = MIPSNES2_DSP_VOICE_RELEASE;
            release_envelope(dsp, index, false);
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
        unsigned int index;

        (void)memset(dsp, 0, sizeof(*dsp));
        /* The internal reset/mute/echo-disable state is distinct from the
         * readable register shadow, whose electrical reset value is not
         * claimed by this foundation. */
        dsp->internal_flags = UINT8_C(0xe0);
        dsp->noise_lfsr = UINT16_C(0x4000);
        for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
            dsp->voices[index].envelope_state =
                MIPSNES2_DSP_ENVELOPE_RELEASE;
        }
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

        result.samples[index] = decode_brr_sample(
            raw, result.shift, result.filter, &previous1, &previous2);
    }
    result.previous1 = previous1;
    result.previous2 = previous2;
    *decoded = result;
    return true;
}

bool mipsnes2_dsp_voice_interpolation_input(
    const mipsnes2_dsp *dsp, size_t voice_index,
    mipsnes2_dsp_interpolation_input *input)
{
    const mipsnes2_dsp_voice *voice;
    uint8_t sample_index;
    unsigned int index;

    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || input == NULL) {
        return false;
    }
    voice = &dsp->voices[voice_index];
    if (!voice->brr_stream_active || voice->key_on_delay != 0U ||
        voice->decoded_groups < 3U) {
        return false;
    }
    sample_index = (uint8_t)(voice->interpolation_index >> 12U);
    for (index = 0U; index < 4U; ++index) {
        input->samples[index] = voice->brr_ring[
            (voice->ring_offset + sample_index + index) %
            MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT];
    }
    input->sample_index = sample_index;
    input->fraction = (uint8_t)(voice->interpolation_index >> 4U);
    return true;
}

bool mipsnes2_dsp_voice_noise_input(const mipsnes2_dsp *dsp,
                                    size_t voice_index, int16_t *sample)
{
    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || sample == NULL ||
        (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
         (uint8_t)(UINT8_C(1) << voice_index)) == 0U) {
        return false;
    }
    *sample = current_noise_sample(dsp);
    return true;
}

bool mipsnes2_dsp_voice_output_sample(const mipsnes2_dsp *dsp,
                                      size_t voice_index, int16_t *sample)
{
    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || sample == NULL ||
        !dsp->voices[voice_index].output_sample_valid) {
        return false;
    }
    *sample = dsp->voices[voice_index].output_sample;
    return true;
}

bool mipsnes2_dsp_voice_stereo_sample(const mipsnes2_dsp *dsp,
                                      size_t voice_index,
                                      mipsnes2_dsp_stereo_sample *sample)
{
    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || sample == NULL ||
        !dsp->voices[voice_index].output_sample_valid) {
        return false;
    }
    sample->left = dsp->voices[voice_index].left_sample;
    sample->right = dsp->voices[voice_index].right_sample;
    return true;
}

bool mipsnes2_dsp_resolved_dry_mix(const mipsnes2_dsp *dsp,
                                   mipsnes2_dsp_dry_mix *mix)
{
    if (dsp == NULL || !dsp->initialized || mix == NULL) {
        return false;
    }
    *mix = dsp->dry_mix;
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
    advance_global_state(dsp, samples);
    if (samples != 0U) {
        refresh_idle_noise_outputs(dsp);
    }
    dsp->sample_count += samples;
    return true;
}
