/* Synthetic DMA address/mode fixture. See LICENSE for GPL-3.0. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "core/dma/transfer.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

int main(void)
{
    static const uint8_t expected[8][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 1, 1 },
        { 0, 1, 2, 3, 0, 1, 2, 3 },
        { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 1, 1 }
    };
    uint8_t offset = UINT8_C(0x5a);
    unsigned mode;
    unsigned index;

    for (mode = 0U; mode < 8U; ++mode) {
        for (index = 0U; index < 8U; ++index) {
            CHECK(mipsnes2_dma_port_offset((uint8_t)mode, index, &offset));
            CHECK(offset == expected[mode][index]);
        }
    }
    CHECK(mipsnes2_dma_port_offset(4U, UINT32_MAX, &offset));
    CHECK(offset == 3U);
    CHECK(!mipsnes2_dma_port_offset(8U, 0U, &offset));
    CHECK(offset == 3U);
    CHECK(!mipsnes2_dma_port_offset(0U, 0U, NULL));
    CHECK(mipsnes2_dma_next_a_offset(UINT16_C(0xffff), 0U) == 0U);
    CHECK(mipsnes2_dma_next_a_offset(0U, 16U) == UINT16_C(0xffff));
    CHECK(mipsnes2_dma_next_a_offset(0U, 8U) == 0U);
    CHECK(mipsnes2_dma_next_a_offset(0U, 24U) == 0U);
    CHECK(mipsnes2_dma_transfer_length(0U) == UINT32_C(65536));
    CHECK(mipsnes2_dma_transfer_length(UINT16_C(0xffff)) ==
          UINT32_C(65535));
    (void)puts("DMA transfer address plan: OK");
    return 0;
}
