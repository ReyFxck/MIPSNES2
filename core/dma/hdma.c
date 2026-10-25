/* Pure SNES HDMA table and scanline engine. See LICENSE for GPL-3.0. */
#include "core/dma/hdma.h"

#include <stddef.h>
#include <string.h>

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

static uint8_t read_table_byte(uint8_t *channel,
                               mipsnes2_hdma_table_read read,
                               void *context,
                               mipsnes2_hdma_report *report)
{
    uint16_t address = word(channel, 8U);
    uint8_t value = read(context,
                         ((mipsnes2_address)channel[4] << 16) | address);

    set_word(channel, 8U, (uint16_t)(address + 1U));
    ++report->table_bytes;
    return value;
}

static void load_indirect_address(uint8_t *channel,
                                  mipsnes2_hdma_table_read read,
                                  void *context,
                                  mipsnes2_hdma_report *report)
{
    uint8_t low = read_table_byte(channel, read, context, report);
    uint8_t high = read_table_byte(channel, read, context, report);

    channel[5] = low;
    channel[6] = high;
}

static bool load_descriptor(mipsnes2_hdma_state *state, unsigned index,
                            mipsnes2_hdma_table_read read, void *context,
                            mipsnes2_hdma_report *report)
{
    uint8_t *channel = state->registers->channel[index];
    uint8_t descriptor = read_table_byte(channel, read, context, report);
    uint8_t bit = (uint8_t)(1U << index);

    channel[10] = descriptor;
    if ((channel[0] & UINT8_C(0x40)) != 0U) {
        load_indirect_address(channel, read, context, report);
    }
    if (descriptor == 0U) {
        state->active_mask &= (uint8_t)~bit;
        state->transfer_mask &= (uint8_t)~bit;
        state->terminated_mask |= bit;
        return false;
    }
    state->active_mask |= bit;
    state->transfer_mask |= bit;
    return true;
}

static uint8_t transfer_bytes(uint8_t mode)
{
    static const uint8_t count[8] = {1U, 2U, 2U, 4U, 4U, 4U, 2U, 4U};

    return count[mode & 7U];
}

void mipsnes2_hdma_state_init(mipsnes2_hdma_state *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

bool mipsnes2_hdma_frame_begin(
    mipsnes2_hdma_state *state, mipsnes2_dma_registers *registers,
    uint8_t enable_mask, mipsnes2_hdma_table_read read, void *context,
    mipsnes2_hdma_report *report)
{
    unsigned index;

    if (state == NULL || registers == NULL || !registers->initialized ||
        read == NULL || report == NULL) {
        return false;
    }
    (void)memset(report, 0, sizeof(*report));
    state->registers = registers;
    state->active_mask = 0U;
    state->transfer_mask = 0U;
    state->known_mask = enable_mask;
    state->terminated_mask = 0U;
    state->initialized = true;
    for (index = 0U; index < 8U; ++index) {
        uint8_t bit = (uint8_t)(1U << index);
        uint8_t *channel;

        if ((enable_mask & bit) == 0U) {
            continue;
        }
        channel = registers->channel[index];
        channel[8] = channel[2];
        channel[9] = channel[3];
        (void)load_descriptor(state, index, read, context, report);
    }
    report->active_mask = state->active_mask;
    return true;
}

bool mipsnes2_hdma_run_scanline(
    mipsnes2_hdma_state *state, uint8_t enable_mask,
    mipsnes2_hdma_table_read read, mipsnes2_dma_byte_transfer transfer,
    void *context, mipsnes2_hdma_report *report)
{
    unsigned index;
    uint8_t new_channels;
    uint8_t eligible;

    if (state == NULL || !state->initialized || state->registers == NULL ||
        !state->registers->initialized || read == NULL || transfer == NULL ||
        report == NULL) {
        return false;
    }
    (void)memset(report, 0, sizeof(*report));
    new_channels = enable_mask & (uint8_t)~state->known_mask &
                   (uint8_t)~state->terminated_mask;
    state->known_mask |= new_channels;
    state->active_mask |= new_channels;
    state->transfer_mask |= new_channels;
    eligible = state->active_mask & enable_mask;

    for (index = 0U; index < 8U; ++index) {
        uint8_t bit = (uint8_t)(1U << index);
        uint8_t *channel;

        if ((eligible & bit) == 0U) {
            continue;
        }
        channel = state->registers->channel[index];
        ++report->channels_visited;
        if ((state->transfer_mask & bit) != 0U) {
            uint8_t count = transfer_bytes(channel[0]);
            uint8_t byte_index;

            for (byte_index = 0U; byte_index < count; ++byte_index) {
                uint16_t a_offset = (channel[0] & UINT8_C(0x40)) != 0U
                    ? word(channel, 5U) : word(channel, 8U);
                uint8_t a_bank = (channel[0] & UINT8_C(0x40)) != 0U
                    ? channel[7] : channel[4];
                uint8_t port_offset;

                (void)mipsnes2_dma_port_offset(
                    channel[0] & 7U, byte_index, &port_offset);
                transfer(context,
                         ((mipsnes2_address)a_bank << 16) | a_offset,
                         UINT32_C(0x2100) |
                             (uint8_t)(channel[1] + port_offset),
                         (channel[0] & UINT8_C(0x80)) != 0U);
                if ((channel[0] & UINT8_C(0x40)) != 0U) {
                    set_word(channel, 5U, (uint16_t)(a_offset + 1U));
                } else {
                    set_word(channel, 8U, (uint16_t)(a_offset + 1U));
                }
                ++report->data_bytes;
            }
        }

        --channel[10];
        if ((channel[10] & UINT8_C(0x80)) != 0U) {
            state->transfer_mask |= bit;
        } else {
            state->transfer_mask &= (uint8_t)~bit;
        }
        if ((channel[10] & UINT8_C(0x7f)) == 0U) {
            (void)load_descriptor(state, index, read, context, report);
        }
    }
    report->active_mask = state->active_mask;
    return true;
}
