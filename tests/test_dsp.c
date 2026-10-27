/* Synthetic S-DSP register/timing foundation fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>

#include "core/apu/dsp/dsp.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

int main(void)
{
    mipsnes2_dsp dsp;
    mipsnes2_dsp chunked;
    uint8_t value = UINT8_C(0xee);

    mipsnes2_dsp_init(&dsp);
    CHECK(dsp.initialized && dsp.internal_flags == UINT8_C(0xe0));
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

    (void)puts("S-DSP register/timing foundation: OK");
    return 0;
}
