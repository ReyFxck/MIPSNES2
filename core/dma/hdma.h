/* Pure SNES HDMA table and scanline engine. See LICENSE for GPL-3.0. */
#ifndef MIPSNES2_DMA_HDMA_H
#define MIPSNES2_DMA_HDMA_H

#include <stdbool.h>
#include <stdint.h>

#include "core/common/address.h"
#include "core/dma/transfer.h"

typedef uint8_t (*mipsnes2_hdma_table_read)(
    void *context, mipsnes2_address address);

typedef struct {
    mipsnes2_dma_registers *registers;
    uint8_t active_mask;
    uint8_t transfer_mask;
    uint8_t known_mask;
    uint8_t terminated_mask;
    bool initialized;
} mipsnes2_hdma_state;

typedef struct {
    uint32_t table_bytes;
    uint32_t data_bytes;
    uint8_t channels_visited;
    uint8_t active_mask;
} mipsnes2_hdma_report;

void mipsnes2_hdma_state_init(mipsnes2_hdma_state *state);

/* Start-of-frame initialization for the channels selected by $420C. A1T is
 * copied to A2A, then the first line descriptor and any indirect pointer are
 * fetched. Table reads are untimed; the scheduler owner supplies bus timing.
 */
bool mipsnes2_hdma_frame_begin(
    mipsnes2_hdma_state *state, mipsnes2_dma_registers *registers,
    uint8_t enable_mask, mipsnes2_hdma_table_read read, void *context,
    mipsnes2_hdma_report *report);

/* Process one visible scanline in channel priority order. Clearing an enable
 * bit pauses an initialized channel without discarding its state. A newly
 * enabled mid-frame channel begins from caller-programmed A2A/NLTR state; a
 * channel terminated by a zero table descriptor cannot restart this frame.
 * Callbacks must not mutate channel registers or reenter this state.
 */
bool mipsnes2_hdma_run_scanline(
    mipsnes2_hdma_state *state, uint8_t enable_mask,
    mipsnes2_hdma_table_read read, mipsnes2_dma_byte_transfer transfer,
    void *context, mipsnes2_hdma_report *report);

#endif
