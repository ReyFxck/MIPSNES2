/* Pure SNES HDMA table and scanline engine. See LICENSE for GPL-3.0. */
#include "core/dma/hdma.h"

#include <stddef.h>
#include <string.h>

enum {
    PHASE_COMPLETE = 0,
    PHASE_GLOBAL,
    PHASE_SELECT,
    PHASE_CHANNEL,
    PHASE_TRANSFER,
    PHASE_FINISH_LINE,
    PHASE_DESCRIPTOR,
    PHASE_INDIRECT_LOW,
    PHASE_INDIRECT_HIGH
};

static uint16_t word(const uint8_t *channel, unsigned low)
{
    return (uint16_t)((uint16_t)channel[low] |
                      ((uint16_t)channel[low + 1U] << 8));
}

static void set_word(uint8_t *channel, unsigned low, uint16_t value)
{
    channel[low] = (uint8_t)value;
    channel[low + 1U] = (uint8_t)(value >> 8);
}

static uint8_t transfer_bytes(uint8_t mode)
{
    static const uint8_t count[8] = {1U, 2U, 2U, 4U, 4U, 4U, 2U, 4U};

    return count[mode & 7U];
}

static bool operation_matches(const mipsnes2_hdma_operation *left,
                              const mipsnes2_hdma_operation *right)
{
    return left->kind == right->kind &&
           left->a_address == right->a_address &&
           left->b_address == right->b_address &&
           left->master_clocks == right->master_clocks &&
           left->channel == right->channel &&
           left->b_to_a == right->b_to_a;
}

static void finish_descriptor(mipsnes2_hdma_cursor *cursor)
{
    uint8_t bit = (uint8_t)(1U << cursor->channel);

    if (cursor->descriptor == 0U) {
        cursor->state->active_mask &= (uint8_t)~bit;
        cursor->state->transfer_mask &= (uint8_t)~bit;
        cursor->state->terminated_mask |= bit;
    } else {
        cursor->state->active_mask |= bit;
        cursor->state->transfer_mask |= bit;
    }
    ++cursor->channel;
    cursor->phase = PHASE_SELECT;
}

static void prepare_operation(mipsnes2_hdma_cursor *cursor,
                              mipsnes2_hdma_operation_kind kind,
                              mipsnes2_address a_address,
                              mipsnes2_address b_address,
                              uint8_t clocks, bool b_to_a)
{
    (void)memset(&cursor->pending_operation, 0,
                 sizeof(cursor->pending_operation));
    cursor->pending_operation.kind = kind;
    cursor->pending_operation.a_address = a_address;
    cursor->pending_operation.b_address = b_address;
    cursor->pending_operation.master_clocks = clocks;
    cursor->pending_operation.channel = cursor->channel;
    cursor->pending_operation.b_to_a = b_to_a;
    cursor->register_snapshot = *cursor->registers;
    cursor->state_snapshot = *cursor->state;
    cursor->operation_pending = true;
}

static mipsnes2_hdma_cursor_result settle(mipsnes2_hdma_cursor *cursor)
{
    while (!cursor->operation_pending && cursor->phase != PHASE_COMPLETE) {
        uint8_t *channel;
        uint8_t bit;

        if (cursor->phase == PHASE_GLOBAL) {
            prepare_operation(cursor, MIPSNES2_HDMA_OPERATION_OVERHEAD,
                              0U, 0U, 18U, false);
            continue;
        }
        if (cursor->phase == PHASE_SELECT) {
            while (cursor->channel < 8U &&
                   (cursor->selected_mask &
                    (uint8_t)(1U << cursor->channel)) == 0U) {
                ++cursor->channel;
            }
            if (cursor->channel == 8U) {
                cursor->phase = PHASE_COMPLETE;
                break;
            }
            channel = cursor->registers->channel[cursor->channel];
            if (cursor->frame_begin) {
                channel[8] = channel[2];
                channel[9] = channel[3];
            }
            cursor->phase = PHASE_CHANNEL;
            continue;
        }
        channel = cursor->registers->channel[cursor->channel];
        bit = (uint8_t)(1U << cursor->channel);
        if (cursor->phase == PHASE_CHANNEL) {
            prepare_operation(cursor, MIPSNES2_HDMA_OPERATION_OVERHEAD,
                              0U, 0U, 8U, false);
            continue;
        }
        if (cursor->phase == PHASE_TRANSFER) {
            uint16_t a_offset = (channel[0] & UINT8_C(0x40)) != 0U
                ? word(channel, 5U) : word(channel, 8U);
            uint8_t a_bank = (channel[0] & UINT8_C(0x40)) != 0U
                ? channel[7] : channel[4];
            uint8_t port_offset;

            (void)mipsnes2_dma_port_offset(
                channel[0] & 7U, cursor->byte_index, &port_offset);
            prepare_operation(
                cursor, MIPSNES2_HDMA_OPERATION_TRANSFER,
                ((mipsnes2_address)a_bank << 16) | a_offset,
                UINT32_C(0x2100) |
                    (uint8_t)(channel[1] + port_offset),
                8U, (channel[0] & UINT8_C(0x80)) != 0U);
            continue;
        }
        if (cursor->phase == PHASE_FINISH_LINE) {
            --channel[10];
            if ((channel[10] & UINT8_C(0x80)) != 0U) {
                cursor->state->transfer_mask |= bit;
            } else {
                cursor->state->transfer_mask &= (uint8_t)~bit;
            }
            if ((channel[10] & UINT8_C(0x7f)) == 0U) {
                cursor->phase = PHASE_DESCRIPTOR;
            } else {
                ++cursor->channel;
                cursor->phase = PHASE_SELECT;
            }
            continue;
        }
        if (cursor->phase == PHASE_DESCRIPTOR ||
            cursor->phase == PHASE_INDIRECT_LOW ||
            cursor->phase == PHASE_INDIRECT_HIGH) {
            prepare_operation(
                cursor, MIPSNES2_HDMA_OPERATION_TABLE_READ,
                ((mipsnes2_address)channel[4] << 16) | word(channel, 8U),
                0U, 8U, false);
            continue;
        }
        return MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT;
    }
    if (cursor->phase == PHASE_COMPLETE) {
        cursor->report.active_mask = cursor->state->active_mask;
        cursor->active = false;
        return MIPSNES2_HDMA_CURSOR_COMPLETE;
    }
    return MIPSNES2_HDMA_CURSOR_READY;
}

void mipsnes2_hdma_state_init(mipsnes2_hdma_state *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

void mipsnes2_hdma_cursor_init(mipsnes2_hdma_cursor *cursor)
{
    if (cursor != NULL) {
        (void)memset(cursor, 0, sizeof(*cursor));
    }
}

mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_begin_frame(
    mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_state *state,
    mipsnes2_dma_registers *registers, uint8_t enable_mask)
{
    if (cursor == NULL || state == NULL || registers == NULL ||
        !registers->initialized || cursor->active) {
        return MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT;
    }
    (void)memset(cursor, 0, sizeof(*cursor));
    state->registers = registers;
    state->active_mask = 0U;
    state->transfer_mask = 0U;
    state->known_mask = enable_mask;
    state->terminated_mask = 0U;
    state->initialized = true;
    cursor->state = state;
    cursor->registers = registers;
    cursor->selected_mask = enable_mask;
    cursor->frame_begin = true;
    cursor->active = true;
    cursor->phase = enable_mask == 0U ? PHASE_COMPLETE : PHASE_GLOBAL;
    return settle(cursor);
}

mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_begin_scanline(
    mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_state *state,
    uint8_t enable_mask)
{
    uint8_t new_channels;

    if (cursor == NULL || state == NULL || !state->initialized ||
        state->registers == NULL || !state->registers->initialized ||
        cursor->active) {
        return MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT;
    }
    (void)memset(cursor, 0, sizeof(*cursor));
    new_channels = enable_mask & (uint8_t)~state->known_mask &
                   (uint8_t)~state->terminated_mask;
    state->known_mask |= new_channels;
    state->active_mask |= new_channels;
    state->transfer_mask |= new_channels;
    cursor->state = state;
    cursor->registers = state->registers;
    cursor->selected_mask = state->active_mask & enable_mask;
    cursor->active = true;
    cursor->phase = cursor->selected_mask == 0U
        ? PHASE_COMPLETE : PHASE_GLOBAL;
    return settle(cursor);
}

mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_peek(
    mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_operation *operation)
{
    mipsnes2_hdma_cursor_result result;

    if (cursor == NULL || operation == NULL || !cursor->active) {
        return MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT;
    }
    result = settle(cursor);
    if (result == MIPSNES2_HDMA_CURSOR_READY) {
        *operation = cursor->pending_operation;
    }
    return result;
}

mipsnes2_hdma_cursor_result mipsnes2_hdma_cursor_commit(
    mipsnes2_hdma_cursor *cursor,
    const mipsnes2_hdma_operation *operation, uint8_t read_value)
{
    uint8_t *channel;

    if (cursor == NULL || operation == NULL || !cursor->active ||
        !cursor->operation_pending ||
        !operation_matches(operation, &cursor->pending_operation)) {
        return MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT;
    }
    if (memcmp(cursor->registers, &cursor->register_snapshot,
               sizeof(*cursor->registers)) != 0 ||
        memcmp(cursor->state, &cursor->state_snapshot,
               sizeof(*cursor->state)) != 0) {
        return MIPSNES2_HDMA_CURSOR_STALE_OPERATION;
    }
    channel = cursor->registers->channel[cursor->channel];
    cursor->operation_pending = false;
    if (operation->kind == MIPSNES2_HDMA_OPERATION_OVERHEAD) {
        if (cursor->phase == PHASE_GLOBAL) {
            cursor->phase = PHASE_SELECT;
        } else {
            ++cursor->report.channels_visited;
            if (cursor->frame_begin) {
                cursor->phase = PHASE_DESCRIPTOR;
            } else if ((cursor->state->transfer_mask &
                 (uint8_t)(1U << cursor->channel)) != 0U) {
                cursor->byte_index = 0U;
                cursor->byte_count = transfer_bytes(channel[0]);
                cursor->phase = PHASE_TRANSFER;
            } else {
                cursor->phase = PHASE_FINISH_LINE;
            }
        }
    } else if (operation->kind == MIPSNES2_HDMA_OPERATION_TRANSFER) {
        uint16_t a_offset = (channel[0] & UINT8_C(0x40)) != 0U
            ? word(channel, 5U) : word(channel, 8U);

        if ((channel[0] & UINT8_C(0x40)) != 0U) {
            set_word(channel, 5U, (uint16_t)(a_offset + 1U));
        } else {
            set_word(channel, 8U, (uint16_t)(a_offset + 1U));
        }
        ++cursor->report.data_bytes;
        ++cursor->byte_index;
        if (cursor->byte_index == cursor->byte_count) {
            cursor->phase = PHASE_FINISH_LINE;
        }
    } else if (operation->kind == MIPSNES2_HDMA_OPERATION_TABLE_READ) {
        uint16_t address = word(channel, 8U);

        set_word(channel, 8U, (uint16_t)(address + 1U));
        ++cursor->report.table_bytes;
        if (cursor->phase == PHASE_DESCRIPTOR) {
            cursor->descriptor = read_value;
            channel[10] = read_value;
            if ((channel[0] & UINT8_C(0x40)) != 0U) {
                cursor->phase = PHASE_INDIRECT_LOW;
            } else {
                finish_descriptor(cursor);
            }
        } else if (cursor->phase == PHASE_INDIRECT_LOW) {
            channel[5] = read_value;
            cursor->phase = PHASE_INDIRECT_HIGH;
        } else {
            channel[6] = read_value;
            finish_descriptor(cursor);
        }
    } else {
        return MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT;
    }
    return settle(cursor);
}

bool mipsnes2_hdma_cursor_report(
    const mipsnes2_hdma_cursor *cursor, mipsnes2_hdma_report *report)
{
    if (cursor == NULL || report == NULL || cursor->state == NULL) {
        return false;
    }
    *report = cursor->report;
    report->active_mask = cursor->state->active_mask;
    return true;
}

static bool run_cursor(mipsnes2_hdma_cursor *cursor,
                       mipsnes2_hdma_table_read read,
                       mipsnes2_dma_byte_transfer transfer,
                       void *context, mipsnes2_hdma_report *report)
{
    mipsnes2_hdma_operation operation;
    mipsnes2_hdma_cursor_result result;

    for (;;) {
        uint8_t value = 0U;

        result = mipsnes2_hdma_cursor_peek(cursor, &operation);
        if (result == MIPSNES2_HDMA_CURSOR_COMPLETE) {
            return mipsnes2_hdma_cursor_report(cursor, report);
        }
        if (result != MIPSNES2_HDMA_CURSOR_READY) {
            return false;
        }
        if (operation.kind == MIPSNES2_HDMA_OPERATION_TABLE_READ) {
            value = read(context, operation.a_address);
        } else if (operation.kind == MIPSNES2_HDMA_OPERATION_TRANSFER) {
            transfer(context, operation.a_address, operation.b_address,
                     operation.b_to_a);
        }
        result = mipsnes2_hdma_cursor_commit(cursor, &operation, value);
        if (result == MIPSNES2_HDMA_CURSOR_COMPLETE) {
            return mipsnes2_hdma_cursor_report(cursor, report);
        }
        if (result != MIPSNES2_HDMA_CURSOR_READY) {
            return false;
        }
    }
}

bool mipsnes2_hdma_frame_begin(
    mipsnes2_hdma_state *state, mipsnes2_dma_registers *registers,
    uint8_t enable_mask, mipsnes2_hdma_table_read read, void *context,
    mipsnes2_hdma_report *report)
{
    mipsnes2_hdma_cursor cursor;
    mipsnes2_hdma_cursor_result result;

    if (read == NULL || report == NULL) {
        return false;
    }
    mipsnes2_hdma_cursor_init(&cursor);
    result = mipsnes2_hdma_cursor_begin_frame(
        &cursor, state, registers, enable_mask);
    if (result == MIPSNES2_HDMA_CURSOR_COMPLETE) {
        return mipsnes2_hdma_cursor_report(&cursor, report);
    }
    return result == MIPSNES2_HDMA_CURSOR_READY &&
           run_cursor(&cursor, read, NULL, context, report);
}

bool mipsnes2_hdma_run_scanline(
    mipsnes2_hdma_state *state, uint8_t enable_mask,
    mipsnes2_hdma_table_read read, mipsnes2_dma_byte_transfer transfer,
    void *context, mipsnes2_hdma_report *report)
{
    mipsnes2_hdma_cursor cursor;
    mipsnes2_hdma_cursor_result result;

    if (read == NULL || transfer == NULL || report == NULL) {
        return false;
    }
    mipsnes2_hdma_cursor_init(&cursor);
    result = mipsnes2_hdma_cursor_begin_scanline(
        &cursor, state, enable_mask);
    if (result == MIPSNES2_HDMA_CURSOR_COMPLETE) {
        return mipsnes2_hdma_cursor_report(&cursor, report);
    }
    return result == MIPSNES2_HDMA_CURSOR_READY &&
           run_cursor(&cursor, read, transfer, context, report);
}
