/* Synthetic DMA channel register fixture. See LICENSE for GPL-3.0. */
#include <stdint.h>
#include <stdio.h>

#include "core/dma/registers.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

int main(void)
{
    mipsnes2_dma_registers registers = {0};
    uint8_t value = UINT8_C(0x5a);
    unsigned channel;
    unsigned index;

    CHECK(!mipsnes2_dma_registers_read(&registers, 0x4300U, &value));
    CHECK(value == UINT8_C(0x5a));
    mipsnes2_dma_registers_init(&registers);
    for (channel = 0U; channel < 8U; ++channel) {
        for (index = 0U; index <= 11U; ++index) {
            CHECK(mipsnes2_dma_registers_read(
                &registers, (uint16_t)(0x4300U + channel * 16U + index),
                &value));
            CHECK(value == UINT8_C(0xff));
        }
        CHECK(mipsnes2_dma_registers_read(
            &registers, (uint16_t)(0x430fU + channel * 16U),
            &value));
        CHECK(value == UINT8_C(0xff));
    }
    CHECK(mipsnes2_dma_registers_write(&registers, 0x4300U, 0x91U));
    CHECK(mipsnes2_dma_registers_write(&registers, 0x437aU, 0x37U));
    CHECK(mipsnes2_dma_registers_write(&registers, 0x433bU, 0x11U));
    CHECK(mipsnes2_dma_registers_read(&registers, 0x433fU, &value));
    CHECK(value == 0x11U);
    CHECK(mipsnes2_dma_registers_write(&registers, 0x433fU, 0x22U));
    CHECK(mipsnes2_dma_registers_read(&registers, 0x433bU, &value));
    CHECK(value == 0x22U);
    CHECK(mipsnes2_dma_registers_read(&registers, 0x4300U, &value));
    CHECK(value == 0x91U);
    CHECK(mipsnes2_dma_registers_read(&registers, 0x437aU, &value));
    CHECK(value == 0x37U);
    value = UINT8_C(0x5a);
    CHECK(!mipsnes2_dma_registers_read(&registers, 0x430cU, &value));
    CHECK(!mipsnes2_dma_registers_read(&registers, 0x437eU, &value));
    CHECK(!mipsnes2_dma_registers_read(&registers, 0x4380U, &value));
    CHECK(!mipsnes2_dma_registers_read(&registers, 0x42ffU, &value));
    CHECK(value == UINT8_C(0x5a));
    CHECK(!mipsnes2_dma_registers_write(&registers, 0x430dU, 1U));
    CHECK(!mipsnes2_dma_registers_read(NULL, 0x4300U, &value));
    CHECK(!mipsnes2_dma_registers_read(&registers, 0x4300U, NULL));
    CHECK(!mipsnes2_dma_registers_write(NULL, 0x4300U, 1U));
    mipsnes2_dma_registers_init(&registers);
    CHECK(mipsnes2_dma_registers_read(&registers, 0x4300U, &value));
    CHECK(value == UINT8_C(0xff));
    (void)puts("DMA channel registers: OK");
    return 0;
}
