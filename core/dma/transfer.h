/* Pure general-purpose DMA address plan. See LICENSE for GPL-3.0. */
#ifndef MIPSNES2_DMA_TRANSFER_H
#define MIPSNES2_DMA_TRANSFER_H

#include <stdbool.h>
#include <stdint.h>

/* BBAD's low byte plus the mode's port offset selects $21xx. */
bool mipsnes2_dma_port_offset(uint8_t mode, uint32_t byte_index,
                              uint8_t *offset);
/* $43x0 bit 3 fixes the A address; otherwise bit 4 decrements it.
 * Only the 16-bit offset changes; the A bank is held by the caller.
 */
uint16_t mipsnes2_dma_next_a_offset(uint16_t offset, uint8_t control);
/* A zero count denotes 65536 bytes for general-purpose DMA. */
uint32_t mipsnes2_dma_transfer_length(uint16_t size);

#endif
