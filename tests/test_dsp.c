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

int main(void)
{
    mipsnes2_dsp dsp;
    mipsnes2_dsp chunked;
    uint8_t value = UINT8_C(0xee);

    mipsnes2_dsp_init(&dsp);
    CHECK(dsp.initialized && dsp.internal_flags == UINT8_C(0xe0));
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
    CHECK(mipsnes2_dsp_advance(&chunked, 7U));
    CHECK(mipsnes2_dsp_advance(&chunked, 25U));
    CHECK(mipsnes2_dsp_advance(&chunked, 968U));
    CHECK(chunked.smp_cycles == dsp.smp_cycles &&
          chunked.sample_count == dsp.sample_count &&
          chunked.sample_phase == dsp.sample_phase);

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

    (void)puts("S-DSP register/timing/voice/BRR foundation: OK");
    return 0;
}
