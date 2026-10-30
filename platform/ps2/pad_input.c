/* MIPSNES2 PS2 libpad acquisition boundary. See LICENSE. */
#include "platform/ps2/pad_input.h"

#include <string.h>

#include <libpad.h>
#include <loadfile.h>

#include "platform/ps2/pad_mapping.h"

enum {
    PS2_PAD_PORT = 0,
    PS2_PAD_SLOT = 0
};

typedef char mipsnes2_ps2_libpad_mask_contract[
    MIPSNES2_PS2_PAD_SELECT == PAD_SELECT &&
    MIPSNES2_PS2_PAD_START == PAD_START &&
    MIPSNES2_PS2_PAD_UP == PAD_UP &&
    MIPSNES2_PS2_PAD_RIGHT == PAD_RIGHT &&
    MIPSNES2_PS2_PAD_DOWN == PAD_DOWN &&
    MIPSNES2_PS2_PAD_LEFT == PAD_LEFT &&
    MIPSNES2_PS2_PAD_L1 == PAD_L1 &&
    MIPSNES2_PS2_PAD_R1 == PAD_R1 &&
    MIPSNES2_PS2_PAD_TRIANGLE == PAD_TRIANGLE &&
    MIPSNES2_PS2_PAD_CIRCLE == PAD_CIRCLE &&
    MIPSNES2_PS2_PAD_CROSS == PAD_CROSS &&
    MIPSNES2_PS2_PAD_SQUARE == PAD_SQUARE ? 1 : -1];

mipsnes2_ps2_pad_input_result mipsnes2_ps2_pad_input_init(
    mipsnes2_ps2_pad_input *input)
{
    if (input == NULL) {
        return MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT;
    }
    (void)memset(input, 0, sizeof(*input));
    if (SifLoadModule("rom0:SIO2MAN", 0, NULL) < 0 ||
        SifLoadModule("rom0:PADMAN", 0, NULL) < 0) {
        return MIPSNES2_PS2_PAD_INPUT_MODULE_ERROR;
    }
    if (padInit(0) == 0) {
        return MIPSNES2_PS2_PAD_INPUT_INIT_ERROR;
    }
    input->initialized = true;
    if (padPortOpen(PS2_PAD_PORT, PS2_PAD_SLOT, input->dma_area) == 0) {
        (void)padEnd();
        input->initialized = false;
        return MIPSNES2_PS2_PAD_INPUT_OPEN_ERROR;
    }
    input->port_open = true;
    return MIPSNES2_PS2_PAD_INPUT_OK;
}

mipsnes2_ps2_pad_input_result mipsnes2_ps2_pad_input_poll(
    mipsnes2_ps2_pad_input *input, uint16_t *snes_word, bool *connected)
{
    struct padButtonStatus buttons;
    int state;

    if (input == NULL || snes_word == NULL || connected == NULL) {
        return MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT;
    }
    *snes_word = 0U;
    *connected = false;
    if (!input->initialized || !input->port_open) {
        return MIPSNES2_PS2_PAD_INPUT_NOT_READY;
    }
    state = padGetState(PS2_PAD_PORT, PS2_PAD_SLOT);
    if (state == PAD_STATE_DISCONN) {
        return MIPSNES2_PS2_PAD_INPUT_DISCONNECTED;
    }
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return MIPSNES2_PS2_PAD_INPUT_NOT_READY;
    }
    if (padRead(PS2_PAD_PORT, PS2_PAD_SLOT, &buttons) == 0U) {
        return MIPSNES2_PS2_PAD_INPUT_READ_ERROR;
    }
    *snes_word = mipsnes2_ps2_pad_translate(buttons.btns);
    *connected = true;
    return MIPSNES2_PS2_PAD_INPUT_OK;
}

void mipsnes2_ps2_pad_input_end(mipsnes2_ps2_pad_input *input)
{
    if (input == NULL) {
        return;
    }
    if (input->port_open) {
        (void)padPortClose(PS2_PAD_PORT, PS2_PAD_SLOT);
    }
    if (input->initialized) {
        (void)padEnd();
    }
    (void)memset(input, 0, sizeof(*input));
}
