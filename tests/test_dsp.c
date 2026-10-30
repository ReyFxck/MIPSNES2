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

static void write_ram_word(uint8_t ram[MIPSNES2_DSP_ARAM_SIZE],
                           uint16_t address, int16_t value)
{
    uint16_t bits = (uint16_t)value;

    ram[address] = (uint8_t)bits;
    ram[(uint16_t)(address + UINT16_C(1))] = (uint8_t)(bits >> 8U);
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
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
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
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
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
    CHECK(write_register(&direct, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
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
                         UINT8_C(0xa0)));
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
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
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
    CHECK(dsp.voices[0].source_sample ==
          chunked.voices[0].source_sample);
    CHECK(dsp.voices[0].output_sample ==
          chunked.voices[0].output_sample);
    CHECK(dsp.voices[0].left_sample ==
          chunked.voices[0].left_sample);
    CHECK(dsp.voices[0].right_sample ==
          chunked.voices[0].right_sample);
    CHECK(dsp.voices[0].output_sample_valid ==
          chunked.voices[0].output_sample_valid);
    CHECK(dsp.voices[0].output_uses_noise ==
          chunked.voices[0].output_uses_noise);
    CHECK(memcmp(&dsp.dry_mix, &chunked.dry_mix,
                 sizeof(dsp.dry_mix)) == 0);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] ==
          chunked.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX]);
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
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
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
                         UINT8_C(0xa0)));
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
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
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
    CHECK(dsp.voices[0].source_sample == INT16_C(-8192));
    CHECK(dsp.voices[0].output_sample == 0);

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

static int test_gaussian(void)
{
    mipsnes2_dsp_interpolation_input input = {
        {INT16_C(1000), INT16_C(-2000), INT16_C(3000),
         INT16_C(-4000)},
        0U,
        0U
    };
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    uint16_t previous = 0U;
    uint16_t coefficient = UINT16_C(0xa5a5);
    int16_t sample = INT16_C(1234);
    unsigned int index;

    CHECK(!mipsnes2_dsp_gaussian_coefficient(0U, NULL));
    CHECK(!mipsnes2_dsp_gaussian_coefficient(512U, &coefficient));
    CHECK(coefficient == UINT16_C(0xa5a5));
    for (index = 0U; index < 512U; ++index) {
        CHECK(mipsnes2_dsp_gaussian_coefficient(index, &coefficient));
        CHECK(coefficient <= UINT16_C(0x0fff));
        CHECK(index == 0U || coefficient >= previous);
        hash ^= coefficient & UINT16_C(0x00ff);
        hash *= UINT64_C(0x100000001b3);
        hash ^= coefficient >> 8U;
        hash *= UINT64_C(0x100000001b3);
        previous = coefficient;
    }
    CHECK(hash == UINT64_C(0xb1216dfae1423abe));
    CHECK(mipsnes2_dsp_gaussian_coefficient(0U, &coefficient));
    CHECK(coefficient == 0U);
    CHECK(mipsnes2_dsp_gaussian_coefficient(15U, &coefficient));
    CHECK(coefficient == 0U);
    CHECK(mipsnes2_dsp_gaussian_coefficient(16U, &coefficient));
    CHECK(coefficient == 1U);
    CHECK(mipsnes2_dsp_gaussian_coefficient(255U, &coefficient));
    CHECK(coefficient == UINT16_C(370));
    CHECK(mipsnes2_dsp_gaussian_coefficient(256U, &coefficient));
    CHECK(coefficient == UINT16_C(374));
    CHECK(mipsnes2_dsp_gaussian_coefficient(511U, &coefficient));
    CHECK(coefficient == UINT16_C(1305));
    for (index = 0U; index < 256U; ++index) {
        uint16_t c0;
        uint16_t c1;
        uint16_t c2;
        uint16_t c3;
        uint32_t sum;

        CHECK(mipsnes2_dsp_gaussian_coefficient(index, &c0));
        CHECK(mipsnes2_dsp_gaussian_coefficient(255U - index, &c1));
        CHECK(mipsnes2_dsp_gaussian_coefficient(256U + index, &c2));
        CHECK(mipsnes2_dsp_gaussian_coefficient(511U - index, &c3));
        sum = (uint32_t)c0 + c1 + c2 + c3;
        CHECK(sum >= UINT32_C(0x07ff));
        CHECK(sum <= UINT32_C(0x0801));
    }

    CHECK(!mipsnes2_dsp_gaussian_interpolate(NULL, &sample));
    CHECK(!mipsnes2_dsp_gaussian_interpolate(&input, NULL));
    input.sample_index = UINT8_C(8);
    CHECK(!mipsnes2_dsp_gaussian_interpolate(&input, &sample));
    CHECK(sample == INT16_C(1234));
    input.sample_index = 0U;
    input.samples[0] = INT16_MIN;
    CHECK(!mipsnes2_dsp_gaussian_interpolate(&input, &sample));
    CHECK(sample == INT16_C(1234));
    input.samples[0] = INT16_C(1000);
    CHECK(mipsnes2_dsp_gaussian_interpolate(&input, &sample));
    CHECK(sample == INT16_C(-547));
    input.fraction = UINT8_C(127);
    CHECK(mipsnes2_dsp_gaussian_interpolate(&input, &sample));
    CHECK(sample == INT16_C(385));
    input.fraction = UINT8_C(255);
    CHECK(mipsnes2_dsp_gaussian_interpolate(&input, &sample));
    CHECK(sample == INT16_C(823));

    for (index = 0U; index < 4U; ++index) {
        input.samples[index] = INT16_C(-16384);
    }
    input.fraction = 0U;
    CHECK(mipsnes2_dsp_gaussian_interpolate(&input, &sample));
    CHECK(sample == INT16_C(0x3ff8));
    return 0;
}

static int test_gaussian_voice_output(void)
{
    uint8_t ram[MIPSNES2_DSP_ARAM_SIZE];
    mipsnes2_dsp dsp;
    mipsnes2_dsp_dry_mix mix;
    mipsnes2_dsp_stereo_sample stereo;
    int16_t sample;
    unsigned int index;

    (void)memset(ram, 0, sizeof(ram));
    ram[UINT16_C(0x1000)] = 0U;
    ram[UINT16_C(0x1001)] = UINT8_C(0x20);
    ram[UINT16_C(0x1002)] = 0U;
    ram[UINT16_C(0x1003)] = UINT8_C(0x20);
    ram[UINT16_C(0x2000)] = UINT8_C(0xc0);
    for (index = 1U; index < MIPSNES2_DSP_BRR_BLOCK_SIZE; ++index) {
        ram[UINT16_C(0x2000) + index] = UINT8_C(0x77);
    }

    mipsnes2_dsp_init(&dsp);
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_DIR,
                         UINT8_C(0x10)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].decoded_groups == UINT64_C(3));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x400));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == 0);

    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(!dsp.voices[0].output_uses_noise);
    CHECK(dsp.voices[0].source_sample == INT16_C(14343));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(14342));
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] ==
          UINT8_C(0x38));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == INT16_C(7171));
    CHECK(stereo.right == INT16_C(7171));
    CHECK(mipsnes2_dsp_resolved_dry_mix(&dsp, &mix));
    CHECK(mix.resolved_voice_mask == UINT8_C(1));
    CHECK(mix.voice_mix.left == INT16_C(7171));
    CHECK(mix.main_sample.left == INT16_C(3585));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].output_uses_noise);
    CHECK(dsp.voices[0].source_sample == INT16_C(8192));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(8192));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, 0U));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(!dsp.voices[0].output_uses_noise);
    CHECK(dsp.voices[0].source_sample == INT16_C(14343));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(14342));
    return 0;
}

static int test_noise_output(void)
{
    mipsnes2_dsp dsp;
    mipsnes2_dsp_stereo_sample stereo;
    int16_t sample = INT16_C(1234);

    mipsnes2_dsp_init(&dsp);
    CHECK(!mipsnes2_dsp_voice_output_sample(NULL, 0U, &sample));
    CHECK(!mipsnes2_dsp_voice_output_sample(
        &dsp, MIPSNES2_DSP_VOICE_COUNT, &sample));
    CHECK(!mipsnes2_dsp_voice_output_sample(&dsp, 0U, NULL));
    CHECK(!mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(1234));
    CHECK(!mipsnes2_dsp_voice_stereo_sample(NULL, 0U, &stereo));
    CHECK(!mipsnes2_dsp_voice_stereo_sample(
        &dsp, MIPSNES2_DSP_VOICE_COUNT, &stereo));
    CHECK(!mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, NULL));
    CHECK(!mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == 0);
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == 0 && stereo.right == 0);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(5) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].envelope == UINT16_C(0x400));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == 0);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] == 0U);

    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].source_sample == INT16_C(-16384));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(-16384));
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] ==
          UINT8_C(0xc0));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == 0 && stereo.right == 0);

    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
        UINT8_C(0x40)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
        UINT8_C(0xc0)));
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(8192));
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] ==
          UINT8_C(0x20));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == INT16_C(4096));
    CHECK(stereo.right == INT16_C(-4096));

    dsp.noise_lfsr = UINT16_C(0x7ffd);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(-4));
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] ==
          UINT8_C(0xff));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == INT16_C(-2));
    CHECK(stereo.right == INT16_C(2));
    dsp.noise_lfsr = UINT16_C(3);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(2));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == INT16_C(1) && stereo.right == INT16_C(-1));

    dsp.noise_lfsr = UINT16_C(0x2000);
    dsp.voices[0].state = MIPSNES2_DSP_VOICE_RELEASE;
    dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_RELEASE;
    dsp.voices[0].envelope = UINT16_C(16);
    dsp.voices[0].envelope_calculation = INT32_C(16);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(128));
    CHECK(dsp.voices[0].envelope == UINT16_C(8));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(64));
    CHECK(dsp.voices[0].envelope == 0U);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == 0);

    dsp.voices[0].state = MIPSNES2_DSP_VOICE_ACTIVE;
    dsp.voices[0].envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
    dsp.voices[0].envelope = UINT16_C(0x400);
    dsp.voices[0].envelope_calculation = INT32_C(0x400);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x80)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == 0);
    CHECK(dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_OUTX] == 0U);
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == 0 && stereo.right == 0);

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, 0U));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(!mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(!mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(3)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                         UINT8_C(0x40)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT, UINT8_C(0x20)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
              MIPSNES2_DSP_VOICE_REGISTER_ADSR1,
        0U));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
              MIPSNES2_DSP_VOICE_REGISTER_GAIN,
        UINT8_C(0x40)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
              MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
        UINT8_C(0x40)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
              MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
        UINT8_C(0xc0)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(3)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 0U, &sample));
    CHECK(sample == INT16_C(8192));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 0U, &stereo));
    CHECK(stereo.left == INT16_C(2048) && stereo.right == 0);
    CHECK(mipsnes2_dsp_voice_output_sample(&dsp, 1U, &sample));
    CHECK(sample == INT16_C(8192));
    CHECK(mipsnes2_dsp_voice_stereo_sample(&dsp, 1U, &stereo));
    CHECK(stereo.left == INT16_C(4096));
    CHECK(stereo.right == INT16_C(-4096));
    return 0;
}

static int test_pitch_modulation(void)
{
    mipsnes2_dsp dsp;

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_PMON, UINT8_C(2)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                  MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW,
        0U));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                  MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH,
        UINT8_C(0x10)));
    dsp.voices[0].envelope = UINT16_C(0x400);
    dsp.voices[1].brr_stream_active = true;
    dsp.voices[1].decoded_groups = UINT64_C(3);
    dsp.voices[1].output_sample_valid = true;
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].output_sample == INT16_C(0x2000));
    CHECK(dsp.voices[1].pitch == UINT16_C(0x1400));
    CHECK(dsp.voices[1].interpolation_index == UINT16_C(0x1400));

    dsp.voices[0].envelope = UINT16_C(0x7ff);
    dsp.voices[1].interpolation_index = 0U;
    dsp.noise_lfsr = UINT16_C(0x4000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].output_sample == INT16_C(-32752));
    CHECK(dsp.voices[1].pitch == 0U);
    CHECK(dsp.voices[1].interpolation_index == 0U);

    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                  MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW,
        UINT8_C(1)));
    dsp.voices[0].envelope = UINT16_C(0x400);
    dsp.voices[1].interpolation_index = 0U;
    dsp.noise_lfsr = UINT16_C(0x7ffd);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].output_sample == INT16_C(-4));
    CHECK(dsp.voices[1].pitch == UINT16_C(0x0ffc));
    CHECK(dsp.voices[1].interpolation_index == UINT16_C(0x0ffc));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(3)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                  MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW,
        0U));
    dsp.voices[0].envelope = UINT16_C(0x400);
    dsp.voices[1].interpolation_index = 0U;
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[1].pitch == UINT16_C(0x1000));
    CHECK(dsp.voices[1].interpolation_index == UINT16_C(0x1000));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                  MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW,
        UINT8_C(0xff)));
    CHECK(write_register(
        &dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                  MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH,
        UINT8_C(0x3f)));
    dsp.voices[0].envelope = UINT16_C(0x7ff);
    dsp.voices[1].interpolation_index = UINT16_C(0x3000);
    dsp.noise_lfsr = UINT16_C(0x3fff);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].output_sample == INT16_C(0x7fee));
    CHECK(dsp.voices[1].pitch == UINT16_C(0x7fee));
    CHECK(dsp.voices[1].interpolation_index == UINT16_C(0x7fff));

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_PMON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH,
                         UINT8_C(0x10)));
    dsp.voices[0].brr_stream_active = true;
    dsp.voices[0].decoded_groups = UINT64_C(3);
    dsp.voices[0].output_sample_valid = true;
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.voices[0].pitch == UINT16_C(0x1000));
    CHECK(dsp.voices[0].interpolation_index == UINT16_C(0x1000));
    return 0;
}

static int test_resolved_dry_mix(void)
{
    mipsnes2_dsp dsp;
    mipsnes2_dsp direct;
    mipsnes2_dsp chunked;
    mipsnes2_dsp_dry_mix mix;
    unsigned int index;

    (void)memset(&mix, UINT8_C(0xa5), sizeof(mix));
    mipsnes2_dsp_init(&dsp);
    CHECK(!mipsnes2_dsp_resolved_dry_mix(NULL, &mix));
    CHECK(!mipsnes2_dsp_resolved_dry_mix(&dsp, NULL));
    CHECK(mipsnes2_dsp_resolved_dry_mix(&dsp, &mix));
    CHECK(mix.resolved_voice_mask == 0U);
    CHECK(mix.voice_mix.left == 0 && mix.voice_mix.right == 0);
    CHECK(mix.main_sample.left == 0 && mix.main_sample.right == 0);

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON,
                         UINT8_C(0x0f)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT,
                         UINT8_C(0xc0)));
    for (index = 0U; index < 4U; ++index) {
        uint8_t base = (uint8_t)(
            index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

        CHECK(write_register(
            &dsp, (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_ADSR1),
            0U));
        CHECK(write_register(
            &dsp, (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_GAIN),
            UINT8_C(0x7f)));
        CHECK(write_register(
            &dsp,
            (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT),
            index < 3U ? UINT8_C(0x7f) : UINT8_C(0x80)));
        CHECK(write_register(
            &dsp,
            (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT),
            index == 0U ? UINT8_C(0x80) : UINT8_C(0x7f)));
    }
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON,
                         UINT8_C(0x0f)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    dsp.noise_lfsr = UINT16_C(0x3fff);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_resolved_dry_mix(&dsp, &mix));
    CHECK(mix.resolved_voice_mask == UINT8_C(0x0f));
    CHECK(dsp.voices[0].output_sample == INT16_C(32510));
    CHECK(dsp.voices[0].left_sample == INT16_C(32256));
    CHECK(dsp.voices[0].right_sample == INT16_C(-32510));
    CHECK(mix.voice_mix.left == INT16_C(257));
    CHECK(mix.voice_mix.right == INT16_MAX);
    CHECK(mix.main_sample.left == INT16_C(128));
    CHECK(mix.main_sample.right == INT16_C(-16384));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                         UINT8_C(0x80)));
    for (index = 0U; index < 4U; ++index) {
        uint8_t reg = (uint8_t)(
            (index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE) +
            MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT);

        CHECK(write_register(&dsp, reg, UINT8_C(0x80)));
    }
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_resolved_dry_mix(&dsp, &mix));
    CHECK(mix.voice_mix.left == INT16_MIN);
    CHECK(mix.main_sample.left == INT16_MIN);

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x80)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_resolved_dry_mix(&dsp, &mix));
    CHECK(mix.resolved_voice_mask == UINT8_C(0x0f));
    CHECK(mix.voice_mix.left == 0 && mix.voice_mix.right == 0);
    CHECK(mix.main_sample.left == 0 && mix.main_sample.right == 0);

    mipsnes2_dsp_init(&dsp);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON,
                         UINT8_C(0xff)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT,
                         UINT8_C(0x40)));
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        uint8_t base = (uint8_t)(
            index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

        CHECK(write_register(
            &dsp, (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_ADSR1),
            0U));
        CHECK(write_register(
            &dsp, (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_GAIN),
            UINT8_C(0x40)));
        CHECK(write_register(
            &dsp,
            (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT),
            UINT8_C(1)));
        CHECK(write_register(
            &dsp,
            (uint8_t)(base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT),
            UINT8_C(0xff)));
    }
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_KON,
                         UINT8_C(0xff)));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_resolved_dry_mix(&dsp, &mix));
    CHECK(mix.resolved_voice_mask == UINT8_C(0xff));
    CHECK(mix.voice_mix.left == INT16_C(512));
    CHECK(mix.voice_mix.right == INT16_C(-512));
    CHECK(mix.main_sample.left == INT16_C(256));
    CHECK(mix.main_sample.right == INT16_C(-256));

    direct = dsp;
    chunked = dsp;
    CHECK(mipsnes2_dsp_advance(&direct, UINT64_C(777)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(13)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(244)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(520)));
    CHECK(direct.sample_count == chunked.sample_count);
    CHECK(direct.sample_phase == chunked.sample_phase);
    CHECK(direct.noise_lfsr == chunked.noise_lfsr);
    CHECK(memcmp(&direct.dry_mix, &chunked.dry_mix,
                 sizeof(direct.dry_mix)) == 0);
    return 0;
}

static int test_echo_fir(void)
{
    static const int16_t normal_samples[MIPSNES2_DSP_FIR_TAP_COUNT] = {
        100, 200, 300, 400, 500, 600, 700, 800
    };
    static const int8_t normal_coefficients[MIPSNES2_DSP_FIR_TAP_COUNT] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };
    int16_t samples[MIPSNES2_DSP_FIR_TAP_COUNT] = {0};
    int8_t coefficients[MIPSNES2_DSP_FIR_TAP_COUNT] = {0};
    int16_t sample = INT16_C(1234);
    unsigned int index;

    CHECK(!mipsnes2_dsp_fir(NULL, normal_coefficients, &sample));
    CHECK(!mipsnes2_dsp_fir(normal_samples, NULL, &sample));
    CHECK(!mipsnes2_dsp_fir(normal_samples, normal_coefficients, NULL));
    CHECK(sample == INT16_C(1234));
    samples[0] = INT16_C(16384);
    CHECK(!mipsnes2_dsp_fir(samples, coefficients, &sample));
    CHECK(sample == INT16_C(1234));

    CHECK(mipsnes2_dsp_fir(normal_samples, normal_coefficients, &sample));
    CHECK(sample == INT16_C(316));
    for (index = 0U; index < MIPSNES2_DSP_FIR_TAP_COUNT; ++index) {
        samples[index] = INT16_C(16383);
        coefficients[index] = index < 7U ? INT8_C(127) : 0;
    }
    CHECK(mipsnes2_dsp_fir(samples, coefficients, &sample));
    CHECK(sample == INT16_C(30962));

    (void)memset(samples, 0, sizeof(samples));
    (void)memset(coefficients, 0, sizeof(coefficients));
    samples[0] = INT16_C(16383);
    samples[7] = INT16_C(16383);
    coefficients[0] = INT8_C(127);
    coefficients[7] = INT8_C(127);
    CHECK(mipsnes2_dsp_fir(samples, coefficients, &sample));
    CHECK(sample == INT16_C(32766));
    samples[0] = INT16_C(-16384);
    samples[7] = INT16_C(-16384);
    CHECK(mipsnes2_dsp_fir(samples, coefficients, &sample));
    CHECK(sample == INT16_MIN);
    return 0;
}

static int test_echo_and_output(void)
{
    uint8_t ram[MIPSNES2_DSP_ARAM_SIZE];
    uint8_t chunked_ram[MIPSNES2_DSP_ARAM_SIZE];
    mipsnes2_dsp dsp;
    mipsnes2_dsp direct;
    mipsnes2_dsp chunked;
    mipsnes2_dsp_echo echo;
    mipsnes2_dsp_output output;
    mipsnes2_pcm_buffer pcm;
    mipsnes2_pcm_frame pcm_storage[1];
    mipsnes2_pcm_frame pcm_output;

    (void)memset(&echo, UINT8_C(0xa5), sizeof(echo));
    (void)memset(&output, UINT8_C(0xa5), sizeof(output));
    mipsnes2_dsp_init(&dsp);
    CHECK(!mipsnes2_dsp_resolved_echo(NULL, &echo));
    CHECK(!mipsnes2_dsp_resolved_echo(&dsp, NULL));
    CHECK(!mipsnes2_dsp_resolved_output(NULL, &output));
    CHECK(!mipsnes2_dsp_resolved_output(&dsp, NULL));
    CHECK(mipsnes2_dsp_resolved_echo(&dsp, &echo));
    CHECK(mipsnes2_dsp_resolved_output(&dsp, &output));
    CHECK(!echo.ram_valid && echo.length == UINT16_C(4));
    CHECK(output.muted && output.dac_sample.left == 0 &&
          output.dac_sample.right == 0);

    (void)memset(ram, 0, sizeof(ram));
    write_ram_word(ram, 0U, INT16_C(2000));
    write_ram_word(ram, 2U, INT16_C(-2000));
    write_ram_word(ram, UINT16_C(0x4000), INT16_C(4000));
    write_ram_word(ram, UINT16_C(0x4002), INT16_C(-4000));
    CHECK(mipsnes2_dsp_attach_ram(&dsp, ram, sizeof(ram)));
    CHECK(dsp.echo.ram_valid);
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_EON, UINT8_C(1)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_ESA,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_EDL, 0U));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FIR_BASE +
                         (7U * MIPSNES2_DSP_VOICE_REGISTER_STRIDE),
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_ECHO_FEEDBACK,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_ECHO_VOLUME_LEFT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_ECHO_VOLUME_RIGHT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT,
                         UINT8_C(0x40)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
                         UINT8_C(0x10)));
    CHECK(write_register(&dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
                         UINT8_C(0xf0)));
    dsp.voices[0].envelope = UINT16_C(0x400);
    dsp.noise_lfsr = UINT16_C(0x2000);
    CHECK(mipsnes2_pcm_buffer_init(&pcm, pcm_storage, 1U));
    CHECK(mipsnes2_dsp_attach_pcm(&dsp, &pcm));

    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_resolved_echo(&dsp, &echo));
    CHECK(mipsnes2_dsp_resolved_output(&dsp, &output));
    CHECK(echo.address == 0U && echo.base_address == UINT16_C(0x4000));
    CHECK(echo.offset == 0U && echo.length == UINT16_C(4));
    CHECK(echo.history_index == 1U && echo.ram_valid &&
          !echo.write_enabled);
    CHECK(echo.fir_sample.left == INT16_C(1000) &&
          echo.fir_sample.right == INT16_C(-1000));
    CHECK(echo.voice_mix.left == INT16_C(1024) &&
          echo.voice_mix.right == INT16_C(-1024));
    CHECK(echo.resolved_voice_mask == UINT8_C(1));
    CHECK(output.echo_sample.left == INT16_C(500) &&
          output.echo_sample.right == INT16_C(-500));
    CHECK(output.mixed_sample.left == INT16_C(1012) &&
          output.mixed_sample.right == INT16_C(-1012));
    CHECK(output.dac_sample.left == INT16_C(1012) && !output.muted);
    CHECK(mipsnes2_pcm_buffer_read(&pcm, &pcm_output, 1U) == 1U);
    CHECK(pcm_output.left == INT16_C(1012) &&
          pcm_output.right == INT16_C(-1012));
    mipsnes2_dsp_detach_pcm(&dsp);
    CHECK(ram[0] == UINT8_C(0xd0) && ram[1] == UINT8_C(0x07));

    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.address == UINT16_C(0x4000));
    CHECK(dsp.echo.fir_sample.left == INT16_C(2000) &&
          dsp.echo.fir_sample.right == INT16_C(-2000));
    CHECK(dsp.echo.feedback_sample.left == INT16_C(1000) &&
          dsp.echo.feedback_sample.right == INT16_C(-1000));
    CHECK(dsp.echo.write_sample.left == INT16_C(2024) &&
          dsp.echo.write_sample.right == INT16_C(-2024));
    CHECK(dsp.output.mixed_sample.left == INT16_C(1512) &&
          dsp.output.mixed_sample.right == INT16_C(-1512));
    CHECK(ram[UINT16_C(0x4000)] == UINT8_C(0xa0) &&
          ram[UINT16_C(0x4001)] == UINT8_C(0x0f));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG, 0U));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.write_enabled);
    CHECK(ram[UINT16_C(0x4000)] == UINT8_C(0xe8) &&
          ram[UINT16_C(0x4001)] == UINT8_C(0x07));
    CHECK(ram[UINT16_C(0x4002)] == UINT8_C(0x18) &&
          ram[UINT16_C(0x4003)] == UINT8_C(0xf8));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x40)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.output.muted && dsp.output.dac_sample.left == 0 &&
          dsp.output.dac_sample.right == 0);
    CHECK(dsp.output.mixed_sample.left == INT16_C(1018) &&
          dsp.output.mixed_sample.right == INT16_C(-1018));
    CHECK(ram[UINT16_C(0x4000)] == UINT8_C(0xfa) &&
          ram[UINT16_C(0x4001)] == UINT8_C(0x05));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_FLG,
                         UINT8_C(0x20)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(!dsp.output.muted && dsp.output.dac_sample.left == INT16_C(894));
    CHECK(!dsp.echo.write_enabled);
    CHECK(ram[UINT16_C(0x4000)] == UINT8_C(0xfa) &&
          ram[UINT16_C(0x4001)] == UINT8_C(0x05));

    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_EDL, UINT8_C(2)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.length == UINT16_C(4096) &&
          dsp.echo.offset == UINT16_C(4));
    CHECK(write_register(&dsp, MIPSNES2_DSP_REGISTER_EDL, UINT8_C(1)));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.length == UINT16_C(4096) &&
          dsp.echo.offset == UINT16_C(8));
    dsp.echo.offset = UINT16_C(4092);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.offset == 0U && dsp.echo.length == UINT16_C(4096));
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.length == UINT16_C(2048) &&
          dsp.echo.offset == UINT16_C(4));

    dsp.echo.base_address = UINT16_C(0xff00);
    dsp.echo.offset = UINT16_C(0x0200);
    dsp.echo.length = UINT16_C(4096);
    CHECK(mipsnes2_dsp_advance(&dsp, MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.echo.address == UINT16_C(0x0100));

    direct = dsp;
    (void)memcpy(chunked_ram, ram, sizeof(ram));
    chunked = direct;
    chunked.ram = chunked_ram;
    CHECK(mipsnes2_dsp_advance(&direct, UINT64_C(777)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(13)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(244)));
    CHECK(mipsnes2_dsp_advance(&chunked, UINT64_C(520)));
    CHECK(direct.sample_count == chunked.sample_count &&
          direct.sample_phase == chunked.sample_phase);
    CHECK(memcmp(&direct.echo, &chunked.echo, sizeof(direct.echo)) == 0);
    CHECK(memcmp(&direct.output, &chunked.output,
                 sizeof(direct.output)) == 0);
    CHECK(memcmp(ram, chunked_ram, sizeof(ram)) == 0);
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

static int test_pcm_handoff(void)
{
    mipsnes2_dsp dsp;
    mipsnes2_dsp chunked;
    mipsnes2_pcm_buffer buffer;
    mipsnes2_pcm_buffer chunked_buffer;
    mipsnes2_pcm_frame storage[4];
    mipsnes2_pcm_frame chunked_storage[4];
    mipsnes2_pcm_frame output[4];
    size_t index;

    mipsnes2_dsp_init(&dsp);
    CHECK(!mipsnes2_dsp_attach_pcm(NULL, &buffer));
    CHECK(!mipsnes2_dsp_attach_pcm(&dsp, NULL));
    CHECK(mipsnes2_pcm_buffer_init(&buffer, storage, 4U));
    CHECK(mipsnes2_dsp_attach_pcm(&dsp, &buffer));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(3) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.sample_count == UINT64_C(3));
    CHECK(buffer.count == 3U && buffer.dropped_frames == 0U);
    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 3U) == 3U);
    for (index = 0U; index < 3U; ++index) {
        CHECK(output[index].left == 0 && output[index].right == 0);
    }

    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.sample_count == UINT64_C(9));
    CHECK(buffer.count == 4U && buffer.dropped_frames == UINT64_C(2));
    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 4U) == 4U);
    for (index = 0U; index < 4U; ++index) {
        CHECK(output[index].left == dsp.output.dac_sample.left);
        CHECK(output[index].right == dsp.output.dac_sample.right);
    }

    mipsnes2_dsp_detach_pcm(&dsp);
    CHECK(dsp.pcm == NULL);
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(2) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(buffer.count == 0U);

    mipsnes2_dsp_init(&dsp);
    chunked = dsp;
    mipsnes2_pcm_buffer_reset(&buffer);
    CHECK(mipsnes2_pcm_buffer_init(
        &chunked_buffer, chunked_storage, 4U));
    CHECK(mipsnes2_dsp_attach_pcm(&dsp, &buffer));
    CHECK(mipsnes2_dsp_attach_pcm(&chunked, &chunked_buffer));
    CHECK(mipsnes2_dsp_advance(
        &dsp, UINT64_C(7) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(mipsnes2_dsp_advance(&chunked, 1U));
    CHECK(mipsnes2_dsp_advance(&chunked, 31U));
    CHECK(mipsnes2_dsp_advance(
        &chunked, UINT64_C(6) * MIPSNES2_DSP_SAMPLE_CYCLES));
    CHECK(dsp.sample_count == chunked.sample_count);
    CHECK(buffer.count == chunked_buffer.count);
    CHECK(buffer.dropped_frames == chunked_buffer.dropped_frames);
    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 4U) == 4U);
    CHECK(mipsnes2_pcm_buffer_read(
        &chunked_buffer, chunked_storage, 4U) == 4U);
    for (index = 0U; index < 4U; ++index) {
        CHECK(output[index].left == chunked_storage[index].left);
        CHECK(output[index].right == chunked_storage[index].right);
    }
    return 0;
}

enum {
    LONG_PCM_HASH_FRAMES = 65536,
    LONG_PCM_FIFO_FRAMES = 128
};

typedef struct {
    uint64_t pcm_hash;
    uint64_t ram_hash;
    uint64_t frame_count;
    uint64_t smp_cycles;
    uint64_t sample_count;
    uint64_t enqueued_frames;
    uint64_t consumed_frames;
    uint64_t dropped_frames;
    uint16_t noise_lfsr;
    uint16_t envelope_counter;
    uint16_t echo_offset;
    int16_t last_left;
    int16_t last_right;
    uint8_t sample_phase;
    uint8_t echo_history_index;
} long_pcm_hash_result;

static uint64_t fnv1a_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ value) * UINT64_C(1099511628211);
}

static uint64_t hash_pcm_frame(uint64_t hash, mipsnes2_pcm_frame frame)
{
    uint16_t left = (uint16_t)frame.left;
    uint16_t right = (uint16_t)frame.right;

    hash = fnv1a_byte(hash, (uint8_t)left);
    hash = fnv1a_byte(hash, (uint8_t)(left >> 8U));
    hash = fnv1a_byte(hash, (uint8_t)right);
    return fnv1a_byte(hash, (uint8_t)(right >> 8U));
}

static bool configure_long_pcm_hash(mipsnes2_dsp *dsp,
                                    uint8_t ram[MIPSNES2_DSP_ARAM_SIZE])
{
    static const uint8_t fir[MIPSNES2_DSP_FIR_TAP_COUNT] = {
        UINT8_C(12), UINT8_C(0xf8), UINT8_C(20), UINT8_C(32),
        UINT8_C(0xf0), UINT8_C(8), UINT8_C(4), UINT8_C(48)
    };
    size_t index;

    (void)memset(ram, 0, MIPSNES2_DSP_ARAM_SIZE);
    for (index = 0U; index < 4096U; ++index) {
        ram[UINT16_C(0x6000) + index] = (uint8_t)(
            (index * 73U) + ((index >> 3U) * 19U) + 0x5aU);
    }
    mipsnes2_dsp_init(dsp);
    if (!mipsnes2_dsp_attach_ram(dsp, ram, MIPSNES2_DSP_ARAM_SIZE) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_FLG, UINT8_C(0x1f)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_NON, UINT8_C(3)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_EON, UINT8_C(1)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_ESA, UINT8_C(0x60)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_EDL, UINT8_C(2)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                        UINT8_C(0x55)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT,
                        UINT8_C(0xb0)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_ECHO_VOLUME_LEFT,
                        UINT8_C(0x28)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_ECHO_VOLUME_RIGHT,
                        UINT8_C(0xe8)) ||
        !write_register(dsp, MIPSNES2_DSP_REGISTER_ECHO_FEEDBACK,
                        UINT8_C(0x35))) {
        return false;
    }
    for (index = 0U; index < MIPSNES2_DSP_FIR_TAP_COUNT; ++index) {
        if (!write_register(
                dsp,
                (uint8_t)(MIPSNES2_DSP_REGISTER_FIR_BASE +
                          (index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE)),
                fir[index])) {
            return false;
        }
    }
    return write_register(dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
                          UINT8_C(0x60)) &&
           write_register(dsp, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
                          UINT8_C(0xc0)) &&
           write_register(dsp, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U) &&
           write_register(dsp, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                          UINT8_C(0x50)) &&
           write_register(
               dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                        MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
               UINT8_C(0xe0)) &&
           write_register(
               dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                        MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
               UINT8_C(0x48)) &&
           write_register(
               dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                        MIPSNES2_DSP_VOICE_REGISTER_ADSR1,
               0U) &&
           write_register(
               dsp, MIPSNES2_DSP_VOICE_REGISTER_STRIDE +
                        MIPSNES2_DSP_VOICE_REGISTER_GAIN,
               UINT8_C(0x68)) &&
           write_register(dsp, MIPSNES2_DSP_REGISTER_KON, UINT8_C(3));
}

static int run_long_pcm_hash(const uint64_t *chunks, size_t chunk_count,
                             long_pcm_hash_result *result)
{
    uint8_t ram[MIPSNES2_DSP_ARAM_SIZE];
    mipsnes2_pcm_frame fifo_storage[LONG_PCM_FIFO_FRAMES];
    mipsnes2_pcm_frame output[LONG_PCM_FIFO_FRAMES];
    mipsnes2_pcm_buffer pcm;
    mipsnes2_dsp dsp;
    uint64_t remaining =
        ((uint64_t)LONG_PCM_HASH_FRAMES * MIPSNES2_DSP_SAMPLE_CYCLES) +
        UINT64_C(19);
    uint64_t pcm_hash = UINT64_C(14695981039346656037);
    uint64_t frame_count = 0U;
    uint64_t ram_hash = UINT64_C(14695981039346656037);
    size_t chunk_index = 0U;
    size_t index;

    CHECK(chunks != NULL && chunk_count != 0U && result != NULL);
    CHECK(configure_long_pcm_hash(&dsp, ram));
    CHECK(mipsnes2_pcm_buffer_init(
        &pcm, fifo_storage, LONG_PCM_FIFO_FRAMES));
    CHECK(mipsnes2_dsp_attach_pcm(&dsp, &pcm));
    while (remaining != 0U) {
        uint64_t cycles = chunks[chunk_index % chunk_count];
        size_t produced;

        CHECK(cycles != 0U);
        if (cycles > remaining) {
            cycles = remaining;
        }
        CHECK(mipsnes2_dsp_advance(&dsp, cycles));
        remaining -= cycles;
        ++chunk_index;
        produced = mipsnes2_pcm_buffer_read(
            &pcm, output, LONG_PCM_FIFO_FRAMES);
        for (index = 0U; index < produced; ++index) {
            pcm_hash = hash_pcm_frame(pcm_hash, output[index]);
        }
        frame_count += produced;
    }
    for (index = 0U; index < MIPSNES2_DSP_ARAM_SIZE; ++index) {
        ram_hash = fnv1a_byte(ram_hash, ram[index]);
    }
    result->pcm_hash = pcm_hash;
    result->ram_hash = ram_hash;
    result->frame_count = frame_count;
    result->smp_cycles = dsp.smp_cycles;
    result->sample_count = dsp.sample_count;
    result->enqueued_frames = pcm.enqueued_frames;
    result->consumed_frames = pcm.consumed_frames;
    result->dropped_frames = pcm.dropped_frames;
    result->noise_lfsr = dsp.noise_lfsr;
    result->envelope_counter = dsp.envelope_counter;
    result->echo_offset = dsp.echo.offset;
    result->last_left = dsp.output.dac_sample.left;
    result->last_right = dsp.output.dac_sample.right;
    result->sample_phase = dsp.sample_phase;
    result->echo_history_index = dsp.echo.history_index;
    CHECK(pcm.count == 0U);
    mipsnes2_dsp_detach_pcm(&dsp);
    return 0;
}

static int test_long_pcm_hash(void)
{
    static const uint64_t regular_chunks[] = {UINT64_C(2048)};
    static const uint64_t irregular_chunks[] = {
        UINT64_C(1), UINT64_C(31), UINT64_C(63), UINT64_C(97),
        UINT64_C(511), UINT64_C(1025), UINT64_C(7), UINT64_C(2047),
        UINT64_C(13), UINT64_C(3)
    };
    long_pcm_hash_result regular;
    long_pcm_hash_result irregular;

    CHECK(run_long_pcm_hash(regular_chunks,
                            sizeof(regular_chunks) /
                                sizeof(regular_chunks[0]),
                            &regular) == 0);
    CHECK(run_long_pcm_hash(irregular_chunks,
                            sizeof(irregular_chunks) /
                                sizeof(irregular_chunks[0]),
                            &irregular) == 0);
    CHECK(regular.pcm_hash == irregular.pcm_hash);
    CHECK(regular.ram_hash == irregular.ram_hash);
    CHECK(regular.frame_count == irregular.frame_count);
    CHECK(regular.smp_cycles == irregular.smp_cycles);
    CHECK(regular.sample_count == irregular.sample_count);
    CHECK(regular.enqueued_frames == irregular.enqueued_frames);
    CHECK(regular.consumed_frames == irregular.consumed_frames);
    CHECK(regular.dropped_frames == irregular.dropped_frames);
    CHECK(regular.noise_lfsr == irregular.noise_lfsr);
    CHECK(regular.envelope_counter == irregular.envelope_counter);
    CHECK(regular.echo_offset == irregular.echo_offset);
    CHECK(regular.last_left == irregular.last_left);
    CHECK(regular.last_right == irregular.last_right);
    CHECK(regular.sample_phase == irregular.sample_phase);
    CHECK(regular.echo_history_index == irregular.echo_history_index);
    CHECK(regular.frame_count == (uint64_t)LONG_PCM_HASH_FRAMES);
    CHECK(regular.sample_count == (uint64_t)LONG_PCM_HASH_FRAMES);
    CHECK(regular.sample_phase == UINT8_C(19));
    CHECK(regular.dropped_frames == 0U);
    if (regular.pcm_hash != UINT64_C(0x368899e61f053bab) ||
        regular.ram_hash != UINT64_C(0x2fa2329372f44e6c)) {
        (void)fprintf(stderr,
                      "long PCM hash: %016llx; ARAM hash: %016llx\n",
                      (unsigned long long)regular.pcm_hash,
                      (unsigned long long)regular.ram_hash);
    }
    CHECK(regular.pcm_hash == UINT64_C(0x368899e61f053bab));
    CHECK(regular.ram_hash == UINT64_C(0x2fa2329372f44e6c));
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
    CHECK(!mipsnes2_dsp_can_advance(&dsp, 1U));
    CHECK(!mipsnes2_dsp_advance(&dsp, 1U));
    CHECK(dsp.smp_cycles == UINT64_MAX);
    CHECK(!mipsnes2_dsp_write_address(NULL, 0U));
    CHECK(!mipsnes2_dsp_read_address(&dsp, NULL));
    CHECK(!mipsnes2_dsp_write_data(NULL, 0U));
    CHECK(!mipsnes2_dsp_read_data(&dsp, NULL));
    CHECK(!mipsnes2_dsp_advance(NULL, 1U));
    CHECK(!mipsnes2_dsp_can_advance(NULL, 1U));

    mipsnes2_dsp_init(&dsp);
    dsp.sample_count = UINT64_MAX;
    dsp.sample_phase = MIPSNES2_DSP_SAMPLE_CYCLES - 1U;
    CHECK(!mipsnes2_dsp_can_advance(&dsp, 1U));
    CHECK(!mipsnes2_dsp_advance(&dsp, 1U));
    CHECK(dsp.smp_cycles == 0U && dsp.sample_count == UINT64_MAX &&
          dsp.sample_phase == MIPSNES2_DSP_SAMPLE_CYCLES - 1U);

    CHECK(test_brr() == 0);
    CHECK(test_voice_control() == 0);
    CHECK(test_voice_stream() == 0);
    CHECK(test_envelope() == 0);
    CHECK(test_noise() == 0);
    CHECK(test_gaussian() == 0);
    CHECK(test_gaussian_voice_output() == 0);
    CHECK(test_noise_output() == 0);
    CHECK(test_pitch_modulation() == 0);
    CHECK(test_resolved_dry_mix() == 0);
    CHECK(test_echo_fir() == 0);
    CHECK(test_echo_and_output() == 0);
    CHECK(test_pcm_handoff() == 0);
    CHECK(test_long_pcm_hash() == 0);

    (void)puts(
        "S-DSP Gaussian, echo/output and deterministic PCM foundation: OK");
    return 0;
}
