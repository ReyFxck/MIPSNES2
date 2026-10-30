/* MIPSNES2 PS2 libpad acquisition boundary. See LICENSE. */
#ifndef MIPSNES2_PS2_PAD_INPUT_H
#define MIPSNES2_PS2_PAD_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MIPSNES2_PS2_PAD_INPUT_OK = 0,
    MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT,
    MIPSNES2_PS2_PAD_INPUT_MODULE_ERROR,
    MIPSNES2_PS2_PAD_INPUT_INIT_ERROR,
    MIPSNES2_PS2_PAD_INPUT_OPEN_ERROR,
    MIPSNES2_PS2_PAD_INPUT_NOT_READY,
    MIPSNES2_PS2_PAD_INPUT_DISCONNECTED,
    MIPSNES2_PS2_PAD_INPUT_READ_ERROR
} mipsnes2_ps2_pad_input_result;

typedef struct {
    uint8_t dma_area[256] __attribute__((aligned(64)));
    bool initialized;
    bool port_open;
} mipsnes2_ps2_pad_input;

mipsnes2_ps2_pad_input_result mipsnes2_ps2_pad_input_init(
    mipsnes2_ps2_pad_input *input);
mipsnes2_ps2_pad_input_result mipsnes2_ps2_pad_input_poll(
    mipsnes2_ps2_pad_input *input, uint16_t *snes_word, bool *connected);
void mipsnes2_ps2_pad_input_end(mipsnes2_ps2_pad_input *input);

#endif
