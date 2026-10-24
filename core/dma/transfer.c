/* Pure general-purpose DMA address plan. See LICENSE for GPL-3.0. */
#include "core/dma/transfer.h"

#include <stddef.h>

bool mipsnes2_dma_port_offset(uint8_t mode, uint32_t byte_index,
                              uint8_t *offset)
{
    static const uint8_t sequence[8][4] = {
        { 0U, 0U, 0U, 0U },
        { 0U, 1U, 0U, 1U },
        { 0U, 0U, 0U, 0U },
        { 0U, 0U, 1U, 1U },
        { 0U, 1U, 2U, 3U },
        { 0U, 1U, 0U, 1U },
        { 0U, 0U, 0U, 0U },
        { 0U, 0U, 1U, 1U }
    };

    if (offset == NULL || mode > 7U) {
        return false;
    }
    *offset = sequence[mode][byte_index & 3U];
    return true;
}

uint16_t mipsnes2_dma_next_a_offset(uint16_t offset, uint8_t control)
{
    if ((control & 8U) != 0U) {
        return offset;
    }
    if ((control & 16U) != 0U) {
        return (uint16_t)(offset - 1U);
    }
    return (uint16_t)(offset + 1U);
}

uint32_t mipsnes2_dma_transfer_length(uint16_t size)
{
    return size == 0U ? UINT32_C(65536) : size;
}
