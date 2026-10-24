/* SNES DMA/HDMA channel register subset. See LICENSE for GPL-3.0. */
#include "core/dma/registers.h"

#include <string.h>

static bool decode(uint16_t offset, unsigned *channel, unsigned *index)
{
    if (offset < UINT16_C(0x4300) || offset > UINT16_C(0x437f)) {
        return false;
    }
    *channel = ((unsigned)offset - 0x4300U) >> 4;
    *index = (unsigned)offset & 15U;
    if (*index == 15U) {
        *index = 11U;
    }
    return *index <= 11U;
}

void mipsnes2_dma_registers_init(mipsnes2_dma_registers *registers)
{
    if (registers == NULL) {
        return;
    }
    (void)memset(registers, 0xff, sizeof(registers->channel));
    registers->initialized = true;
}

bool mipsnes2_dma_registers_read(const mipsnes2_dma_registers *registers,
                                 uint16_t offset, uint8_t *value)
{
    unsigned channel;
    unsigned index;

    if (registers == NULL || !registers->initialized || value == NULL ||
        !decode(offset, &channel, &index)) {
        return false;
    }
    *value = registers->channel[channel][index];
    return true;
}

bool mipsnes2_dma_registers_write(mipsnes2_dma_registers *registers,
                                  uint16_t offset, uint8_t value)
{
    unsigned channel;
    unsigned index;

    if (registers == NULL || !registers->initialized ||
        !decode(offset, &channel, &index)) {
        return false;
    }
    registers->channel[channel][index] = value;
    return true;
}
