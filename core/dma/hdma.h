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

typedef enum {
    MIPSNES2_HDMA_OPERATION_OVERHEAD = 0,
    MIPSNES2_HDMA_OPERATION_TABLE_READ,
    MIPSNES2_HDMA_OPERATION_TRANSFER
} mipsnes2_hdma_operation_kind;

typedef struct {
    mipsnes2_hdma_operation_kind kind;
    mipsnes2_address a_address;
    mipsnes2_address b_address;
    uint8_t master_clocks;
    uint8_t channel;
    bool b_to_a;
} mipsnes2_hdma_operation;

typedef enum {
    MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT = 0,
    MIPSNES2_HDMA_CURSOR_READY,
    MIPSNES2_HDMA_CURSOR_COMPLETE,
    MIPSNES2_HDMA_CURSOR_STALE_OPERATION
} mipsnes2_hdma_cursor_result;

/* Caller-owned operation cursor. Fields are private to hdma.c. */
typedef struct {
    mipsnes2_hdma_state *state;
    mipsnes2_dma_registers *registers;
    mipsnes2_dma_registers register_snapshot;
    mipsnes2_hdma_state state_snapshot;
    mipsnes2_hdma_operation pending_operation;
    mipsnes2_hdma_report report;
    uint8_t selected_mask;
    uint8_t channel;
    uint8_t phase;
    uint8_t byte_index;
    uint8_t byte_count;
    uint8_t descriptor;
    bool frame_begin;
    bool active;
    bool operation_pending;
} mipsnes2_hdma_cursor;

void mipsnes2_hdma_state_init(mipsnes2_hdma_state *state);
/* VBlank ends current-frame HDMA without changing $420C or table registers. */
bool mipsnes2_hdma_state_deactivate(mipsnes2_hdma_state *state);
void mipsnes2_hdma_cursor_init(mipsnes2_hdma_cursor *cursor);

/* Operation-granular form used by the scheduler. OVERHEAD operations have no
 * bus effect. TABLE_READ commits the supplied read_value. TRANSFER describes
 * one A/B byte effect that the caller must perform exactly once before commit.
 */
mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_begin_frame(
    mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_state *state,
    mipsnes2_dma_registers *registers, uint8_t enable_mask);
mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_begin_scanline(
    mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_state *state,
    uint8_t enable_mask);
mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_peek(
    mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_operation *operation);
mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_commit(
    mipsnes2_hdma_cursor *cursor,
    const mipsnes2_hdma_operation *operation, uint8_t read_value);
bool mipsnes2_hdma_cursor_report(
    const mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_report *report);

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
