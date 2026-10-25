/* Pure general-purpose DMA address plan. See LICENSE for GPL-3.0. */
#ifndef MIPSNES2_DMA_TRANSFER_H
#define MIPSNES2_DMA_TRANSFER_H

#include <stdbool.h>
#include <stdint.h>
#include "core/common/address.h"
#include "core/dma/registers.h"

/* BBAD's low byte plus the mode's port offset selects $21xx. */
bool mipsnes2_dma_port_offset(uint8_t mode, uint32_t byte_index,
                              uint8_t *offset);
/* $43x0 bit 3 fixes the A address; otherwise bit 4 decrements it.
 * Only the 16-bit offset changes; the A bank is held by the caller.
 */
uint16_t mipsnes2_dma_next_a_offset(uint16_t offset, uint8_t control);
/* A zero count denotes 65536 bytes for general-purpose DMA. */
uint32_t mipsnes2_dma_transfer_length(uint16_t size);

/* One callback represents one SNES DMA data phase on the A and B buses.
 * It must complete the byte transfer exactly once without mutating the DMA
 * registers or reentering the cursor. The caller supplies CPU suspension,
 * scheduler arbitration, timing and bus-specific access restrictions.
 */
typedef void (*mipsnes2_dma_byte_transfer)(
    void *context, mipsnes2_address a_address, mipsnes2_address b_address,
    bool b_to_a);

typedef struct {
    mipsnes2_dma_registers *registers;
    uint32_t remaining;
    uint32_t byte_index;
    uint8_t pending_mask;
    uint8_t channel;
    bool initialized;
} mipsnes2_dma_cursor;

typedef enum {
    MIPSNES2_DMA_RUN_INVALID_ARGUMENT = 0,
    MIPSNES2_DMA_RUN_PAUSED,
    MIPSNES2_DMA_RUN_COMPLETE
} mipsnes2_dma_run_result;

bool mipsnes2_dma_cursor_begin(mipsnes2_dma_cursor *cursor,
                               mipsnes2_dma_registers *registers,
                               uint8_t channel_mask);
/* HDMA ownership cancels overlapping general-DMA channels at a phase
 * boundary. Reports the channels removed, selecting the next survivor. */
bool mipsnes2_dma_cursor_cancel_channels(mipsnes2_dma_cursor *cursor,
                                         uint8_t mask,
                                         uint8_t *cancelled_mask);
/* Bounded byte effects, channel 0 first. Updates A1T/DAS after each callback.
 * This is an untimed engine; do not call it from CPU execution without the
 * scheduler/bus-ownership layer. A zero byte_budget performs no effects.
 */
mipsnes2_dma_run_result mipsnes2_dma_cursor_run(
    mipsnes2_dma_cursor *cursor, uint32_t byte_budget,
    mipsnes2_dma_byte_transfer transfer, void *context,
    uint32_t *bytes_done);

#endif
