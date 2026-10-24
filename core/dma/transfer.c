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

static void select_channel(mipsnes2_dma_cursor *cursor)
{
    unsigned channel;

    for (channel = 0U; channel < 8U; ++channel) {
        if ((cursor->pending_mask & (1U << channel)) != 0U) {
            uint8_t *state = cursor->registers->channel[channel];
            uint16_t size = (uint16_t)((uint16_t)state[5] |
                                       ((uint16_t)state[6] << 8));

            cursor->channel = (uint8_t)channel;
            cursor->remaining = mipsnes2_dma_transfer_length(size);
            cursor->byte_index = 0U;
            return;
        }
    }
    cursor->channel = 8U;
    cursor->remaining = 0U;
}

bool mipsnes2_dma_cursor_begin(mipsnes2_dma_cursor *cursor,
                               mipsnes2_dma_registers *registers,
                               uint8_t channel_mask)
{
    if (cursor == NULL || registers == NULL || !registers->initialized) {
        return false;
    }
    cursor->registers = registers;
    cursor->pending_mask = channel_mask;
    cursor->initialized = true;
    select_channel(cursor);
    return true;
}

mipsnes2_dma_run_result mipsnes2_dma_cursor_run(
    mipsnes2_dma_cursor *cursor, uint32_t byte_budget,
    mipsnes2_dma_byte_transfer transfer, void *context,
    uint32_t *bytes_done)
{
    uint32_t done = 0U;

    if (cursor == NULL || !cursor->initialized ||
        cursor->registers == NULL || !cursor->registers->initialized ||
        transfer == NULL || bytes_done == NULL) {
        return MIPSNES2_DMA_RUN_INVALID_ARGUMENT;
    }
    while (cursor->pending_mask != 0U && done < byte_budget) {
        uint8_t *state = cursor->registers->channel[cursor->channel];
        uint8_t port_offset;
        uint16_t a_offset = (uint16_t)((uint16_t)state[2] |
                                       ((uint16_t)state[3] << 8));
        uint16_t size = (uint16_t)((uint16_t)state[5] |
                                   ((uint16_t)state[6] << 8));
        mipsnes2_address a_address = ((mipsnes2_address)state[4] << 16) |
                                      a_offset;
        mipsnes2_address b_address;

        (void)mipsnes2_dma_port_offset(state[0] & 7U,
                                       cursor->byte_index, &port_offset);
        b_address = UINT32_C(0x2100) |
                    (uint8_t)(state[1] + port_offset);
        transfer(context, a_address, b_address,
                 (state[0] & UINT8_C(0x80)) != 0U);
        a_offset = mipsnes2_dma_next_a_offset(a_offset, state[0]);
        --size;
        state[2] = (uint8_t)a_offset;
        state[3] = (uint8_t)(a_offset >> 8);
        state[5] = (uint8_t)size;
        state[6] = (uint8_t)(size >> 8);
        ++cursor->byte_index;
        --cursor->remaining;
        ++done;
        if (cursor->remaining == 0U) {
            cursor->pending_mask &= (uint8_t)~(1U << cursor->channel);
            select_channel(cursor);
        }
    }
    *bytes_done = done;
    return cursor->pending_mask == 0U ? MIPSNES2_DMA_RUN_COMPLETE
                                       : MIPSNES2_DMA_RUN_PAUSED;
}
