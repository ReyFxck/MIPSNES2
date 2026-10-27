/* Synthetic S-DSP register/timing foundation fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/dsp/dsp.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static bool write_register(mipsnes2_dsp *dsp, uint8_t reg, uint8_t value)
{
    return mipsnes2_dsp_write_address(dsp, reg) &&
           mipsnes2_dsp_write_data(dsp, value);
}

static int test_brr(void)
{
    static const uint8_t ramp[] = {
        UINT8_C(0x10), UINT8_C(0x01), UINT8_C(0x23), UINT8_C(0x45),
        UINT8_C(0x67), UINT8_C(0x89), UINT8_C(0xab), UINT8_C(0xcd),
        UINT8_C(0xef)
    };
    static const int16_t shift_one[] = {
        0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1
    };
    static const int16_t shift_zero[] = {
        0, 0, 1, 1, 2, 2, 3, 3, -4, -4, -3, -3, -2, -2, -1, -1
    };
    uint8_t block[MIPSNES2_DSP_BRR_BLOCK_SIZE] = {0};
    mipsnes2_dsp_brr_block decoded;
    unsigned int index;

    decoded.header = UINT8_C(0xaa);
    CHECK(!mipsnes2_dsp_brr_decode_block(
        NULL, sizeof(block), 0, 0, &decoded));
    CHECK(!mipsnes2_dsp_brr_decode_block(
        block, sizeof(block) - 1U, 0, 0, &decoded));
    CHECK(!mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 0, 0, NULL));
    CHECK(decoded.header == UINT8_C(0xaa));

    CHECK(mipsnes2_dsp_brr_decode_block(
        ramp, sizeof(ramp), 0, 0, &decoded));
    CHECK(decoded.shift == 1U && decoded.filter == 0U);
    CHECK(!decoded.end && !decoded.loop);
    for (index = 0U; index < MIPSNES2_DSP_BRR_SAMPLE_COUNT; ++index) {
        CHECK(decoded.samples[index] == shift_one[index]);
    }
    block[0] = 0U;
    (void)memcpy(block + 1U, ramp + 1U, sizeof(block) - 1U);
    CHECK(mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 0, 0, &decoded));
    for (index = 0U; index < MIPSNES2_DSP_BRR_SAMPLE_COUNT; ++index) {
        CHECK(decoded.samples[index] == shift_zero[index]);
    }

    (void)memset(block, UINT8_C(0x7f), sizeof(block));
    block[0] = UINT8_C(0xd3);
    CHECK(mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 0, 0, &decoded));
    CHECK(decoded.shift == 13U && decoded.filter == 0U);
    CHECK(decoded.end && decoded.loop);
    CHECK(decoded.samples[0] == 0 && decoded.samples[1] == -2048);

    (void)memset(block, 0, sizeof(block));
    block[0] = UINT8_C(0x04);
    CHECK(mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 1600, 0, &decoded));
    CHECK(decoded.samples[0] == 1500 && decoded.samples[1] == 1406);
    block[0] = UINT8_C(0x08);
    CHECK(mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 1024, 512, &decoded));
    CHECK(decoded.samples[0] == 1472);
    block[0] = UINT8_C(0x0c);
    CHECK(mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 1024, 512, &decoded));
    CHECK(decoded.samples[0] == 1424);

    (void)memset(block, 0, sizeof(block));
    block[0] = UINT8_C(0xc4);
    block[1] = UINT8_C(0x70);
    CHECK(mipsnes2_dsp_brr_decode_block(
        block, sizeof(block), 16383, 0, &decoded));
    CHECK(decoded.samples[0] == -3073);
    return 0;
}

static int test_voice_control(void)
{
    uint8_t ram[MIPSNES2_DSP_ARAM_SIZE];
    mipsnes2_dsp dsp;
    mipsnes2_dsp direct;
    mipsnes2_dsp chunked;

    (void)memset(ram, 0, sizeof(ram));
    ram[UINT16_C(0x200c)] = UINT8_C(0x56);
    ram[UINT16_C(0x200d)] = UINT8_C(0x34);
    ram[UINT16_C(0x200e)] = UINT8_C(0x9a);
    ram[UINT16_C(0x200f)] = UINT8_C(0x78);
    mipsnes2_dsp_init(&dsp);
    CHECK(!mipsnes2_dsp_attach_ram(NULL, ram, sizeof(ram)));
    CHECK(!mipsnes2_dsp_attach_ram(&dsp, NULL, sizeof(ram)));
    CHECK(!mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram) - 1U));
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_DIR, UINT8_C(0x20)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_SRCN,
                         UINT8_C(3)));
    dsp.registers[MIPSNES2_DSP_REGISTER_ENDX] = UINT8_C(0xa5);
    dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_ENVX] = UINT8_C(0xff);
    dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] = UINT8_C(0xff);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(dsp.pending_key_on == UINT8_C(1));
    CHECK(mipsnes2_dsp_advance(&dsp, 31U));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_WAITING);
    CHECK(mipsnes2_dsp_advance(&dsp, 1U));
    CHECK(dsp.pending_key_on == 0U);
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY);
    CHECK(dsp.voices[0].key_on_delay ==
          MIPSNES2_DSP_KEY_ON_DELAY_SAMPLES);
    CHECK(dsp.voices[0].directory_loaded);
    CHECK(dsp.voices[0].source_number == UINT8_C(3));
    CHECK(dsp.voices[0].directory_address == UINT16_C(0x200c));
    CHECK(dsp.voices[0].source_start_address == UINT16_C(0x3456));
    CHECK(dsp.voices[0].source_loop_address == UINT16_C(0x789a));
    CHECK(dsp.registers[MIPSNES2_DSP_REGISTER_ENDX] == UINT8_C(0xa4));
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_ENVX] == 0U);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] == 0U);
    CHECK(mipsnes2_dsp_advance(
        &dsp, MIPSNES2_DSP_KEY_ON_DELAY_SAMPLES *
              MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_ACTIVE);
    CHECK(dsp.voices[0].key_on_delay == 0U);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KOFF, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_RELEASE);

    mipsnes2_dsp_init(&dsp);
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(2)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_WAITING);
    CHECK(dsp.voices[1].state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KOFF, UINT8_C(2)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[1].state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[1].state == MIPSNES2_DSP_VOICE_RELEASE);

    mipsnes2_dsp_init(&direct);
    CHECK(mipsnes2_dsp_attach_ram(&direct, ram, sizeof(ram)));
    CHECK(write_register(&direct, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&direct, MIPSNES2_DSP_REGISTER_KON,
                         UINT8_C(0x81)));
    chunked = direct;
    CHECK(mipsnes2_dsp_advance(&direct, UINT64_C(384)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(17)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(79)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(288)));
    CHECK(direct.sample_count == chunked.sample_count);
    CHECK(direct.sample_phase == chunked.sample_phase);
    CHECK(direct.pending_key_on == chunked.pending_key_on);
    CHECK(direct.voices[0].state == chunked.voices[0].state);
    CHECK(direct.voices[7].state == chunked.voices[7].state);
    CHECK(direct.voices[0].key_on_delay ==
          chunked.voices[0].key_on_delay);
    CHECK(write_register(&direct, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x80)));
    CHECK(mipsnes2_dsp_advance(&direct, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(direct.voices[0].state == MIPSNES2_DSP_VOICE_WAITING);
    CHECK(direct.voices[7].state == MIPSNES2_DSP_VOICE_WAITING);
    return 0;
}

static int test_voice_stream(void)
{
    static const uint8_t ramp_data[8] = {
        UINT8_C(0x01), UINT8_C(0x23), UINT8_C(0x45), UINT8_C(0x67),
        UINT8_C(0x89), UINT8_C(0xab), UINT8_C(0xcd), UINT8_C(0xef)
    };
    uint8_t ram[MIPSNES2_DSP_ARAM_SIZE];
    mipsnes2_dsp dsp;
    mipsnes2_dsp chunked;
    mipsnes2_dsp_interpolation_input input;
    int16_t noise;
    unsigned int index;

    (void)memset(ram, 0, sizeof(ram));
    ram[UINT16_C(0x1014)] = 0U;
    ram[UINT16_C(0x1015)] = UINT8_C(0x30);
    ram[UINT16_C(0x1016)] = UINT8_C(0x09);
    ram[UINT16_C(0x1017)] = UINT8_C(0x30);
    ram[UINT16_C(0x1118)] = 0U;
    ram[UINT16_C(0x1119)] = UINT8_C(0x40);
    ram[UINT16_C(0x111a)] = 0U;
    ram[UINT16_C(0x111b)] = UINT8_C(0x40);
    ram[UINT16_C(0x3000)] = UINT8_C(0x10);
    (void)memcpy(ram + UINT16_C(0x3001), ramp_data,
                 sizeof(ramp_data));
    ram[UINT16_C(0x3009)] = UINT8_C(0x13);
    (void)memset(ram + UINT16_C(0x300a), UINT8_C(0x11), 8U);

    mipsnes2_dsp_init(&dsp);
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(!mipsnes2_dsp_voice_interpolation_input(
        NULL, 0U, &input));
    CHECK(!mipsnes2_dsp_voice_interpolation_input(
        &dsp, MIPSNES2_DSP_VOICE_COUNT, &input));
    CHECK(!mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, NULL));
    CHECK(!mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, &input));
    dsp.voices[0].brr_ring[10] = 123;
    dsp.voices[0].brr_ring[11] = -456;
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_voice_noise_input(&dsp, 0U, &noise));
    CHECK(noise == INT16_C(-16384));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_DIR, UINT8_C(0x10)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_SRCN,
                         UINT8_C(5)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW,
                         UINT8_C(0x80)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH,
                         UINT8_C(0xd0)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].brr_stream_active);
    CHECK(dsp.voices[0].brr_previous1 == -456 &&
          dsp.voices[0].brr_previous2 == 123);
    CHECK(dsp.voices[0].brr_ring[10] == 123 &&
          dsp.voices[0].brr_ring[11] == -456);
    CHECK(!mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, &input));
    CHECK(mipsnes2_dsp_advance(
        &dsp, MIPSNES2_DSP_KEY_ON_DELAY_SAMPLES *
              MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(3));
    CHECK(dsp.voices[0].brr_group_index == 3U);
    CHECK(dsp.voices[0].brr_address == UINT16_C(0x3000));
    CHECK(dsp.voices[0].ring_offset == 0U);
    CHECK(dsp.voices[0].ring_write_offset == 0U);
    CHECK(dsp.voices[0].brr_previous1 == -5);
    CHECK(dsp.voices[0].brr_previous2 == -6);
    CHECK(mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, &input));
    CHECK(input.sample_index == 0U && input.fraction == 0U);
    CHECK(input.samples[0] == 0 && input.samples[1] == 1 &&
          input.samples[2] == 2 && input.samples[3] == 3);

    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].pitch == UINT16_C(0x1080));
    CHECK(dsp.voices[0].interpolation_index == UINT16_C(0x1080));
    CHECK(mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, &input));
    CHECK(input.sample_index == 1U && input.fraction == UINT8_C(8));
    CHECK(input.samples[0] == 1 && input.samples[1] == 2 &&
          input.samples[2] == 3 && input.samples[3] == 4);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(3) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].interpolation_index == UINT16_C(0x4200));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(3));
    CHECK(mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, &input));
    CHECK(input.sample_index == 4U && input.fraction == UINT8_C(0x20));
    CHECK(input.samples[0] == 4 && input.samples[1] == 5 &&
          input.samples[2] == 6 && input.samples[3] == 7);

    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(4));
    CHECK(dsp.voices[0].brr_group_index == 0U);
    CHECK(dsp.voices[0].brr_address == UINT16_C(0x3009));
    CHECK(dsp.voices[0].ring_offset == 4U);
    CHECK(dsp.voices[0].interpolation_index == UINT16_C(0x1280));
    CHECK(dsp.voices[0].brr_previous1 == -1);
    CHECK(dsp.voices[0].brr_previous2 == -2);
    CHECK(mipsnes2_dsp_voice_interpolation_input(&dsp, 0U, &input));
    CHECK(input.sample_index == 1U && input.fraction == UINT8_C(0x28));
    CHECK(input.samples[0] == 5 && input.samples[1] == 6 &&
          input.samples[2] == 7 && input.samples[3] == -8);

    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_SRCN,
                         UINT8_C(6)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_DIR, UINT8_C(0x11)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(16) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(8));
    CHECK(dsp.voices[0].brr_group_index == 0U);
    CHECK(dsp.voices[0].brr_address == UINT16_C(0x4000));
    CHECK(dsp.voices[0].source_number == UINT8_C(6));
    CHECK(dsp.voices[0].directory_address == UINT16_C(0x1118));
    CHECK(dsp.voices[0].source_loop_address == UINT16_C(0x4000));
    CHECK((dsp.registers[MIPSNES2_DSP_REGISTER_ENDX] & UINT8_C(1)) != 0U);
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_ACTIVE);

    chunked = dsp;
    CHECK(mipsnes2_dsp_advance(&dsp, UINT64_C(777)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(13)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(244)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(520)));
    CHECK(dsp.sample_count == chunked.sample_count &&
          dsp.sample_phase == chunked.sample_phase);
    CHECK(dsp.voices[0].interpolation_index ==
          chunked.voices[0].interpolation_index);
    CHECK(dsp.voices[0].decoded_groups ==
          chunked.voices[0].decoded_groups);
    CHECK(dsp.envelope_counter == chunked.envelope_counter);
    CHECK(dsp.noise_lfsr == chunked.noise_lfsr);
    CHECK(dsp.voices[0].envelope == chunked.voices[0].envelope);
    CHECK(dsp.voices[0].envelope_calculation ==
          chunked.voices[0].envelope_calculation);
    CHECK(dsp.voices[0].envelope_state ==
          chunked.voices[0].envelope_state);
    CHECK(dsp.voices[0].brr_address == chunked.voices[0].brr_address &&
          dsp.voices[0].brr_group_index ==
          chunked.voices[0].brr_group_index);
    CHECK(memcmp(dsp.voices[0].brr_ring,
                 chunked.voices[0].brr_ring,
                 sizeof(dsp.voices[0].brr_ring)) == 0);

    mipsnes2_dsp_init(&dsp);
    (void)memset(ram, 0, sizeof(ram));
    ram[UINT16_C(0x1008)] = 0U;
    ram[UINT16_C(0x1009)] = UINT8_C(0x50);
    ram[UINT16_C(0x100a)] = 0U;
    ram[UINT16_C(0x100b)] = UINT8_C(0x50);
    ram[UINT16_C(0x5000)] = UINT8_C(0x11);
    for (index = 0U; index < sizeof(ramp_data); ++index) {
        ram[UINT16_C(0x5001) + index] = ramp_data[index];
    }
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_DIR, UINT8_C(0x10)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_SRCN,
                         UINT8_C(2)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH,
                         UINT8_C(0x10)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(3) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_RELEASE);
    CHECK(dsp.voices[0].key_on_delay == 3U);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(3) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].key_on_delay == 0U);
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(3));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_RELEASE);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(5) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(4));
    CHECK(dsp.voices[0].brr_address == UINT16_C(0x5000));
    CHECK((dsp.registers[MIPSNES2_DSP_REGISTER_ENDX] & UINT8_C(1)) != 0U);
    CHECK(dsp.voices[0].brr_stream_active);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x80)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(4) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_WAITING);
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(5));

    mipsnes2_dsp_init(&dsp);
    (void)memset(ram, 0, sizeof(ram));
    ram[UINT16_C(0x2004)] = UINT8_C(0xfc);
    ram[UINT16_C(0x2005)] = UINT8_C(0xff);
    ram[UINT16_C(0x2006)] = UINT8_C(0xfc);
    ram[UINT16_C(0x2007)] = UINT8_C(0xff);
    ram[UINT16_C(0xfffc)] = UINT8_C(0x10);
    for (index = 0U; index < sizeof(ramp_data); ++index) {
        ram[(uint16_t)(UINT16_C(0xfffd) + (uint16_t)index)] =
            ramp_data[index];
    }
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_DIR, UINT8_C(0x20)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_SRCN,
                         UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH,
                         UINT8_C(0x10)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(3));
    CHECK(dsp.voices[0].brr_address == UINT16_C(0xfffc));
    CHECK(dsp.voices[0].brr_ring[0] == 0 &&
          dsp.voices[0].brr_ring[7] == 7 &&
          dsp.voices[0].brr_ring[8] == -8 &&
          dsp.voices[0].brr_ring[11] == -5);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(5) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(4));
    CHECK(dsp.voices[0].brr_group_index == 0U);
    CHECK(dsp.voices[0].brr_address == UINT16_C(0x0005));
    CHECK(dsp.voices[0].brr_previous1 == -1 &&
          dsp.voices[0].brr_previous2 == -2);
    return 0;
}

static int test_noise(void)
{
    static const uint16_t initial_sequence[] = {
        UINT16_C(0x2000), UINT16_C(0x1000), UINT16_C(0x0800),
        UINT16_C(0x0400), UINT16_C(0x0200), UINT16_C(0x0100),
        UINT16_C(0x0080), UINT16_C(0x0040), UINT16_C(0x0020),
        UINT16_C(0x0010), UINT16_C(0x0008), UINT16_C(0x0004),
        UINT16_C(0x0002), UINT16_C(0x4001), UINT16_C(0x6000)
    };
    mipsnes2_dsp dsp;
    mipsnes2_dsp direct;
    mipsnes2_dsp chunked;
    int16_t sample = INT16_C(1234);
    uint16_t expected;
    unsigned int index;

    mipsnes2_dsp_init(&dsp);
    CHECK(dsp.noise_lfsr == UINT16_C(0x4000));
    CHECK(!mipsnes2_dsp_voice_noise_input(NULL, 0U, &sample));
    CHECK(!mipsnes2_dsp_voice_noise_input(
        &dsp, MIPSNES2_DSP_VOICE_COUNT, &sample));
    CHECK(!mipsnes2_dsp_voice_noise_input(&dsp, 0U, NULL));
    CHECK(!mipsnes2_dsp_voice_noise_input(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(1234));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON,
                         UINT8_C(0x09)));
    CHECK(mipsnes2_dsp_voice_noise_input(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(-16384));
    sample = 0;
    CHECK(mipsnes2_dsp_voice_noise_input(&dsp, 3U, &sample));
    CHECK(sample == INT16_C(-16384));
    CHECK(!mipsnes2_dsp_voice_noise_input(&dsp, 1U, &sample));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(4096) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.noise_lfsr == UINT16_C(0x4000));

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(31)));
    for (index = 0U;
         index < sizeof(initial_sequence) / sizeof(initial_sequence[0]);
         ++index) {
        CHECK(mipsnes2_dsp_advance(
            &dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
        CHECK(dsp.noise_lfsr == initial_sequence[index]);
    }
    CHECK(mipsnes2_dsp_voice_noise_input(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(-8192));

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(2047) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.noise_lfsr == UINT16_C(0x4000));
    CHECK(mipsnes2_dsp_advance(
        &dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.noise_lfsr == UINT16_C(0x2000));

    mipsnes2_dsp_init(&dsp);
    dsp.envelope_counter = UINT16_C(498);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(2)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.envelope_counter == UINT16_C(497));
    CHECK(dsp.noise_lfsr == UINT16_C(0x4000));
    CHECK(mipsnes2_dsp_advance(
        &dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.envelope_counter == UINT16_C(496));
    CHECK(dsp.noise_lfsr == UINT16_C(0x2000));

    mipsnes2_dsp_init(&direct);
    CHECK(write_register(&direct, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(31)));
    chunked = direct;
    CHECK(mipsnes2_dsp_advance(&direct, UINT64_C(32000031)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(13)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(999999)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(31000019)));
    expected = UINT16_C(0x4000);
    for (index = 0U; index < 1000000U; ++index) {
        uint16_t feedback = (uint16_t)(
            ((expected ^ (expected >> 1U)) & UINT16_C(1)) << 14U);

        expected = (uint16_t)((expected >> 1U) | feedback);
    }
    CHECK(direct.smp_cycles == chunked.smp_cycles);
    CHECK(direct.sample_count == chunked.sample_count);
    CHECK(direct.sample_phase == chunked.sample_phase);
    CHECK(direct.envelope_counter == chunked.envelope_counter);
    CHECK(direct.noise_lfsr == chunked.noise_lfsr);
    CHECK(direct.noise_lfsr == expected);
    return 0;
}

static int test_envelope(void)
{
    static const uint16_t expected_rates[32] = {
        0U, 2048U, 1536U, 1280U, 1024U, 768U, 640U, 512U,
        384U, 320U, 256U, 192U, 160U, 128U, 96U, 80U,
        64U, 48U, 40U, 32U, 24U, 20U, 16U, 12U,
        10U, 8U, 6U, 5U, 4U, 3U, 2U, 1U
    };
    static const uint16_t expected_offsets[32] = {
        0U, 0U, 1040U, 536U, 0U, 1040U, 536U, 0U,
        1040U, 536U, 0U, 1040U, 536U, 0U, 1040U, 536U,
        0U, 1040U, 536U, 0U, 1040U, 536U, 0U, 1040U,
        536U, 0U, 1040U, 536U, 0U, 1040U, 0U, 0U
    };
    mipsnes2_dsp dsp;
    uint16_t before_release;
    unsigned int rate;

    mipsnes2_dsp_init(&dsp);
    CHECK(dsp.envelope_counter == 0U);
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_RELEASE);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0x45)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == 0U);
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_ATTACK);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(4) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].key_on_delay == 1U);
    CHECK(dsp.voices[0].envelope == 0U);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_ACTIVE);
    CHECK(dsp.voices[0].envelope == UINT16_C(0x450));
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_ENVX] ==
          UINT8_C(0x45));
    CHECK(dsp.envelope_counter == UINT16_C(0x77fa));

    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0x9f)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x430));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0xbf)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x42b));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0xdf)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x44b));

    dsp.voices[0].envelope = UINT16_C(0x5f0);
    dsp.voices[0].envelope_calculation = INT32_C(0x5f0);
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0xff)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x610));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x618));

    if ((dsp.sample_count & UINT64_C(1)) != 0U) {
        CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    }
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KOFF, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_RELEASE);
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_RELEASE);
    before_release = dsp.voices[0].envelope;
    CHECK(before_release >= 8U);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == before_release - 8U);
    dsp.voices[0].envelope = 4U;
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == 0U);

    dsp.voices[0].state = MIPSNES2_DSP_VOICE_ACTIVE;
    dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
    dsp.voices[0].envelope = UINT16_C(0x700);
    dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_ENVX] = UINT8_C(0x70);
    dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] = UINT8_C(0x55);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x80)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_WAITING);
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_RELEASE);
    CHECK(dsp.voices[0].envelope == 0U);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_ENVX] == 0U);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] == 0U);

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1,
                         UINT8_C(0x8f)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR2,
                         UINT8_C(0xff)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x400));
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_ATTACK);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x7ff));
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_DECAY);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_ENVX] ==
          UINT8_C(0x7f));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x7ff));
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_SUSTAIN);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x7f7));

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    dsp.voices[0].state = MIPSNES2_DSP_VOICE_ACTIVE;
    dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
    dsp.envelope_counter = UINT16_C(497);
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0xc2)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.envelope_counter == UINT16_C(496));
    CHECK(dsp.voices[0].envelope == UINT16_C(32));
    dsp.voices[0].envelope = 0U;
    dsp.voices[0].envelope_calculation = 0;
    dsp.envelope_counter = UINT16_C(498);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == 0U);
    CHECK(dsp.voices[0].envelope_calculation == INT32_C(32));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(32));

    for (rate = 1U; rate < 32U; ++rate) {
        uint16_t event_counter = (uint16_t)(
            (expected_rates[rate] -
             (expected_offsets[rate] % expected_rates[rate])) %
            expected_rates[rate]);

        dsp.voices[0].envelope = 0U;
        dsp.voices[0].envelope_calculation = 0;
        dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
        dsp.envelope_counter = (uint16_t)(
            (event_counter + 1U) % UINT16_C(0x7800));
        CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                             (uint8_t)(UINT8_C(0xc0) | rate)));
        CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
        CHECK(dsp.envelope_counter == event_counter);
        CHECK(dsp.voices[0].envelope == UINT16_C(32));
    }

    dsp.voices[0].envelope = UINT16_C(0x500);
    dsp.voices[0].envelope_calculation = INT32_C(0x500);
    dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_DECAY;
    dsp.envelope_counter = 1U;
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1,
                         UINT8_C(0x80)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR2,
                         UINT8_C(0x9f)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x4fb));
    CHECK(dsp.voices[0].envelope_state ==
          MIPSNES2_DSP_ENVELOPE_SUSTAIN);

    dsp.voices[0].envelope = 0U;
    dsp.voices[0].envelope_calculation = 0;
    dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0xc0)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(100) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == 0U);
    CHECK(dsp.voices[0].envelope_calculation == INT32_C(32));
    return 0;
}

int main(void)
{
    mipsnes2_dsp dsp;
    mipsnes2_dsp chunked;
    uint8_t value = UINT8_C(0xee);

    mipsnes2_dsp_init(&dsp);
    CHECK(dsp.initialized && dsp.internal_flags == UINT8_C(0xe0));
    CHECK(dsp.noise_lfsr == UINT16_C(0x4000));
    CHECK(dsp.voices[0].state == MIPSNES2_DSP_VOICE_WAITING);
    CHECK(dsp.registers[MIPSNES2_DSP_REGISTER_FLG] == 0U);
    CHECK(mipsnes2_dsp_write_address(&dsp, UINT8_C(0x0c)));
    CHECK(mipsnes2_dsp_read_address(&dsp, &value));
    CHECK(value == UINT8_C(0x0c));
    CHECK(mipsnes2_dsp_write_data(&dsp, UINT8_C(0x72)));
    value = 0U;
    CHECK(mipsnes2_dsp_read_data(&dsp, &value));
    CHECK(value == UINT8_C(0x72));

    CHECK(mipsnes2_dsp_write_address(&dsp, UINT8_C(0x8c)));
    CHECK(mipsnes2_dsp_write_data(&dsp, UINT8_C(0x19)));
    CHECK(mipsnes2_dsp_read_data(&dsp, &value));
    CHECK(value == UINT8_C(0x72));
    CHECK(mipsnes2_dsp_read_address(&dsp, &value));
    CHECK(value == UINT8_C(0x8c));

    CHECK(mipsnes2_dsp_write_address(
        &dsp, MIPSNES2_DSP_REGISTER_FLG));
    CHECK(mipsnes2_dsp_write_data(&dsp, UINT8_C(0x35)));
    CHECK(dsp.internal_flags == UINT8_C(0x35));
    dsp.registers[MIPSNES2_DSP_REGISTER_ENDX] = UINT8_C(0xa5);
    CHECK(mipsnes2_dsp_write_address(
        &dsp, MIPSNES2_DSP_REGISTER_ENDX));
    CHECK(mipsnes2_dsp_write_data(&dsp, UINT8_C(0xff)));
    CHECK(dsp.registers[MIPSNES2_DSP_REGISTER_ENDX] == 0U);

    mipsnes2_dsp_init(&dsp);
    chunked = dsp;
    CHECK(mipsnes2_dsp_advance(&dsp, UINT64_C(1000)));
    CHECK(dsp.smp_cycles == UINT64_C(1000));
    CHECK(dsp.sample_count == UINT64_C(31) && dsp.sample_phase == 8U);
    CHECK(dsp.envelope_counter == UINT16_C(0x77e1));
    CHECK(mipsnes2_dsp_advance(&chunked, 7U));
    CHECK(mipsnes2_dsp_advance(&chunked, 25U));
    CHECK(mipsnes2_dsp_advance(&chunked, 968U));
    CHECK(chunked.smp_cycles == dsp.smp_cycles &&
          chunked.sample_count == dsp.sample_count &&
          chunked.sample_phase == dsp.sample_phase &&
          chunked.envelope_counter == dsp.envelope_counter &&
          chunked.noise_lfsr == dsp.noise_lfsr);

    dsp.smp_cycles = UINT64_MAX;
    CHECK(!mipsnes2_dsp_advance(&dsp, 1U));
    CHECK(dsp.smp_cycles == UINT64_MAX);
    CHECK(!mipsnes2_dsp_write_address(NULL, 0U));
    CHECK(!mipsnes2_dsp_read_address(&dsp, NULL));
    CHECK(!mipsnes2_dsp_write_data(NULL, 0U));
    CHECK(!mipsnes2_dsp_read_data(&dsp, NULL));
    CHECK(!mipsnes2_dsp_advance(NULL, 1U));

    CHECK(test_brr() == 0);
    CHECK(test_voice_control() == 0);
    CHECK(test_voice_stream() == 0);
    CHECK(test_envelope() == 0);
    CHECK(test_noise() == 0);

    (void)puts("S-DSP BRR, pitch, envelope and noise foundation: OK");
    return 0;
}
