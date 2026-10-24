/* SNES DMA/HDMA channel register subset. See LICENSE for GPL-3.0. */
#ifndef MIPSNES2_DMA_REGISTERS_H
#define MIPSNES2_DMA_REGISTERS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t channel[8][12]; /* $43x0-$43xB; $43xF aliases xB. */
    bool initialized;
} mipsnes2_dma_registers;

/* Power-on state only. CPU RESET retention and transfer side effects are
 * separate behaviors, and no DMA is started by this component.
 */
void mipsnes2_dma_registers_init(mipsnes2_dma_registers *registers);
/* Offsets $4300-$437F. False means unhandled; read leaves value untouched. */
bool mipsnes2_dma_registers_read(const mipsnes2_dma_registers *registers,
                                 uint16_t offset, uint8_t *value);
bool mipsnes2_dma_registers_write(mipsnes2_dma_registers *registers,
                                  uint16_t offset, uint8_t value);

#endif
