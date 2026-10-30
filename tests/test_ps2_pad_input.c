/* Host lifecycle regression for the PS2 libpad adapter. See LICENSE. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <libpad.h>

#include "core/bus/cpu_io.h"
#include "platform/ps2/pad_input.h"
#include "platform/ps2/pad_mapping.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static int module_result[2];
static unsigned module_calls;
static int init_result;
static int open_result;
static int pad_state;
static unsigned char read_result;
static uint16_t read_buttons;
static unsigned end_calls;
static unsigned close_calls;

int SifLoadModule(const char *path, int arg_len, const char *args)
{
    unsigned index = module_calls < 2U ? module_calls : 1U;

    CHECK(path != NULL);
    CHECK(arg_len == 0 && args == NULL);
    ++module_calls;
    return module_result[index];
}

int padInit(int mode)
{
    CHECK(mode == 0);
    return init_result;
}

int padEnd(void)
{
    ++end_calls;
    return 1;
}

int padPortOpen(int port, int slot, void *pad_area)
{
    CHECK(port == 0 && slot == 0 && pad_area != NULL);
    CHECK(((uintptr_t)pad_area & (uintptr_t)63U) == 0U);
    return open_result;
}

int padPortClose(int port, int slot)
{
    CHECK(port == 0 && slot == 0);
    ++close_calls;
    return 1;
}

unsigned char padRead(int port, int slot, struct padButtonStatus *data)
{
    CHECK(port == 0 && slot == 0 && data != NULL);
    data->btns = read_buttons;
    return read_result;
}

int padGetState(int port, int slot)
{
    CHECK(port == 0 && slot == 0);
    return pad_state;
}

static void reset_stubs(void)
{
    module_result[0] = 1;
    module_result[1] = 2;
    module_calls = 0U;
    init_result = 1;
    open_result = 1;
    pad_state = PAD_STATE_STABLE;
    read_result = 1U;
    read_buttons = UINT16_MAX;
    end_calls = 0U;
    close_calls = 0U;
}

int main(void)
{
    mipsnes2_ps2_pad_input input;
    mipsnes2_ps2_pad_input_result result;
    uint16_t word = UINT16_MAX;
    bool connected = true;

    CHECK(mipsnes2_ps2_pad_input_init(NULL) ==
          MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT);
    reset_stubs();
    module_result[0] = -1;
    CHECK(mipsnes2_ps2_pad_input_init(&input) ==
          MIPSNES2_PS2_PAD_INPUT_MODULE_ERROR);
    CHECK(module_calls == 1U && !input.initialized && !input.port_open);

    reset_stubs();
    module_result[1] = -1;
    CHECK(mipsnes2_ps2_pad_input_init(&input) ==
          MIPSNES2_PS2_PAD_INPUT_MODULE_ERROR);
    CHECK(module_calls == 2U && !input.initialized && !input.port_open);

    reset_stubs();
    init_result = 0;
    CHECK(mipsnes2_ps2_pad_input_init(&input) ==
          MIPSNES2_PS2_PAD_INPUT_INIT_ERROR);
    CHECK(!input.initialized && !input.port_open);

    reset_stubs();
    open_result = 0;
    CHECK(mipsnes2_ps2_pad_input_init(&input) ==
          MIPSNES2_PS2_PAD_INPUT_OPEN_ERROR);
    CHECK(end_calls == 1U && !input.initialized && !input.port_open);

    reset_stubs();
    CHECK(mipsnes2_ps2_pad_input_init(&input) ==
          MIPSNES2_PS2_PAD_INPUT_OK);
    CHECK(input.initialized && input.port_open && module_calls == 2U);
    CHECK(mipsnes2_ps2_pad_input_poll(NULL, &word, &connected) ==
          MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT);
    CHECK(mipsnes2_ps2_pad_input_poll(&input, NULL, &connected) ==
          MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT);
    CHECK(mipsnes2_ps2_pad_input_poll(&input, &word, NULL) ==
          MIPSNES2_PS2_PAD_INPUT_INVALID_ARGUMENT);

    input.port_open = false;
    result = mipsnes2_ps2_pad_input_poll(&input, &word, &connected);
    CHECK(result == MIPSNES2_PS2_PAD_INPUT_NOT_READY);
    CHECK(word == 0U && !connected);
    input.port_open = true;

    pad_state = PAD_STATE_DISCONN;
    CHECK(mipsnes2_ps2_pad_input_poll(&input, &word, &connected) ==
          MIPSNES2_PS2_PAD_INPUT_DISCONNECTED);
    CHECK(word == 0U && !connected);
    pad_state = PAD_STATE_ERROR;
    CHECK(mipsnes2_ps2_pad_input_poll(&input, &word, &connected) ==
          MIPSNES2_PS2_PAD_INPUT_NOT_READY);
    pad_state = PAD_STATE_STABLE;
    read_result = 0U;
    CHECK(mipsnes2_ps2_pad_input_poll(&input, &word, &connected) ==
          MIPSNES2_PS2_PAD_INPUT_READ_ERROR);
    CHECK(word == 0U && !connected);

    pad_state = PAD_STATE_FINDCTP1;
    read_result = 1U;
    read_buttons = (uint16_t)~(
        MIPSNES2_PS2_PAD_CROSS | MIPSNES2_PS2_PAD_UP |
        MIPSNES2_PS2_PAD_R1);
    CHECK(mipsnes2_ps2_pad_input_poll(&input, &word, &connected) ==
          MIPSNES2_PS2_PAD_INPUT_OK);
    CHECK(word == (MIPSNES2_JOYPAD_B | MIPSNES2_JOYPAD_UP |
                   MIPSNES2_JOYPAD_R));
    CHECK(connected);

    mipsnes2_ps2_pad_input_end(&input);
    CHECK(close_calls == 1U && end_calls == 1U);
    CHECK(!input.initialized && !input.port_open);
    mipsnes2_ps2_pad_input_end(NULL);
    (void)puts("PS2 libpad lifecycle adapter: OK");
    return 0;
}
