/* MIPSNES2 S-DSP register/timing foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_DSP_H
#define MIPSNES2_DSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MIPSNES2_DSP_REGISTER_COUNT = 128,
    MIPSNES2_DSP_VOICE_COUNT = 8,
    MIPSNES2_DSP_BRR_BLOCK_SIZE = 9,
    MIPSNES2_DSP_BRR_SAMPLE_COUNT = 16,
    MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT = 12,
    MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT = 4,
    MIPSNES2_DSP_ARAM_SIZE = 65536,
    MIPSNES2_DSP_SAMPLE_CYCLES = 32,
    MIPSNES2_DSP_KEY_ON_DELAY_SAMPLES = 5,
    MIPSNES2_DSP_VOICE_REGISTER_STRIDE = 16,
    MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW = 0x02,
    MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH = 0x03,
    MIPSNES2_DSP_VOICE_REGISTER_SRCN = 0x04,
    MIPSNES2_DSP_VOICE_REGISTER_ADSR1 = 0x05,
    MIPSNES2_DSP_VOICE_REGISTER_ADSR2 = 0x06,
    MIPSNES2_DSP_VOICE_REGISTER_GAIN = 0x07,
    MIPSNES2_DSP_VOICE_REGISTER_ENVX = 0x08,
    MIPSNES2_DSP_VOICE_REGISTER_OUTX = 0x09,
    MIPSNES2_DSP_REGISTER_KON = 0x4c,
    MIPSNES2_DSP_REGISTER_KOFF = 0x5c,
    MIPSNES2_DSP_REGISTER_DIR = 0x5d,
    MIPSNES2_DSP_REGISTER_FLG = 0x6c,
    MIPSNES2_DSP_REGISTER_ENDX = 0x7c
};

typedef enum {
    MIPSNES2_DSP_VOICE_WAITING = 0,
    MIPSNES2_DSP_VOICE_KEY_ON_DELAY,
    MIPSNES2_DSP_VOICE_ACTIVE,
    MIPSNES2_DSP_VOICE_RELEASE
} mipsnes2_dsp_voice_state;

typedef enum {
    MIPSNES2_DSP_ENVELOPE_ATTACK = 0,
    MIPSNES2_DSP_ENVELOPE_DECAY,
    MIPSNES2_DSP_ENVELOPE_SUSTAIN,
    MIPSNES2_DSP_ENVELOPE_RELEASE
} mipsnes2_dsp_envelope_state;

typedef struct {
    int16_t brr_ring[MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT];
    int16_t brr_previous1;
    int16_t brr_previous2;
    uint16_t directory_address;
    uint16_t source_start_address;
    uint16_t source_loop_address;
    uint16_t brr_address;
    uint16_t interpolation_index;
    uint16_t pitch;
    uint16_t envelope;
    uint64_t decoded_groups;
    int32_t envelope_calculation;
    uint8_t source_number;
    uint8_t key_on_delay;
    uint8_t brr_group_index;
    uint8_t brr_header;
    uint8_t ring_offset;
    uint8_t ring_write_offset;
    mipsnes2_dsp_voice_state state;
    mipsnes2_dsp_envelope_state envelope_state;
    bool directory_loaded;
    bool brr_stream_active;
} mipsnes2_dsp_voice;

typedef struct {
    int16_t samples[MIPSNES2_DSP_BRR_SAMPLE_COUNT];
    int16_t previous1;
    int16_t previous2;
    uint8_t header;
    uint8_t shift;
    uint8_t filter;
    bool end;
    bool loop;
} mipsnes2_dsp_brr_block;

typedef struct {
    int16_t samples[4];
    uint8_t sample_index;
    uint8_t fraction;
} mipsnes2_dsp_interpolation_input;

/* State is intentionally public for deterministic inspection. Gaussian output,
 * envelope multiplication, mixing, echo and PCM are not yet implemented. */
typedef struct {
    uint8_t registers[MIPSNES2_DSP_REGISTER_COUNT];
    mipsnes2_dsp_voice voices[MIPSNES2_DSP_VOICE_COUNT];
    uint8_t *ram;
    size_t ram_size;
    uint8_t address;
    uint8_t internal_flags;
    uint8_t pending_key_on;
    uint8_t sample_phase;
    uint16_t envelope_counter;
    uint64_t smp_cycles;
    uint64_t sample_count;
    bool initialized;
} mipsnes2_dsp;

void mipsnes2_dsp_init(mipsnes2_dsp *dsp);
bool mipsnes2_dsp_attach_ram(mipsnes2_dsp *dsp, uint8_t *ram,
                             size_t ram_size);
bool mipsnes2_dsp_write_address(mipsnes2_dsp *dsp, uint8_t address);
bool mipsnes2_dsp_read_address(const mipsnes2_dsp *dsp, uint8_t *address);
bool mipsnes2_dsp_write_data(mipsnes2_dsp *dsp, uint8_t value);
bool mipsnes2_dsp_read_data(const mipsnes2_dsp *dsp, uint8_t *value);

/* Decode one documented nine-byte BRR block, high nibble first. The two
 * history samples are inputs and their updated values are returned. */
bool mipsnes2_dsp_brr_decode_block(const uint8_t *encoded,
                                   size_t encoded_size,
                                   int16_t previous1, int16_t previous2,
                                   mipsnes2_dsp_brr_block *decoded);

/* Return the four logical BRR-ring samples and 8-bit fractional position that
 * would feed the future Gaussian interpolator. */
bool mipsnes2_dsp_voice_interpolation_input(
    const mipsnes2_dsp *dsp, size_t voice_index,
    mipsnes2_dsp_interpolation_input *input);

/* Advance the S-DSP's fixed one-sample-per-32-S-SMP-cycle cadence. */
bool mipsnes2_dsp_advance(mipsnes2_dsp *dsp, uint64_t smp_cycles);

#endif
