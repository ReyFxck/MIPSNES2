/* MIPSNES2 Ricoh 5A22 CPU-I/O tests. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/cpu_io.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static int test_initialization_and_timer_registers(void)
{
    mipsnes2_cpu_io io;
    mipsnes2_cpu_io before;
    uint8_t value = UINT8_C(0x5a);

    (void)memset(&io, 0xa5, sizeof(io));
    mipsnes2_cpu_io_init(NULL, UINT8_C(2));
    mipsnes2_cpu_io_init(&io, UINT8_C(0xf2));
    CHECK(io.htime == UINT16_C(0x01ff));
    CHECK(io.vtime == UINT16_C(0x01ff));
    CHECK(io.cpu_version == UINT8_C(2));
    CHECK(io.nmitimen == 0U && !io.nmi_flag && !io.irq_flag);
    CHECK(!io.vblank && !io.hblank && !io.auto_joypad_busy);
    CHECK(!io.nmi_edge_pending);

    CHECK(!mipsnes2_cpu_io_read(NULL, UINT16_C(0x4210), 0U, &value));
    CHECK(value == UINT8_C(0x5a));
    CHECK(!mipsnes2_cpu_io_read(&io, UINT16_C(0x4210), 0U, NULL));
    CHECK(!mipsnes2_cpu_io_write(NULL, UINT16_C(0x4200), 0U));
    before = io;
    CHECK(!mipsnes2_cpu_io_read(&io, UINT16_C(0x420b), 0U, &value));
    CHECK(value == UINT8_C(0x5a));
    CHECK(!mipsnes2_cpu_io_write(&io, UINT16_C(0x420c), UINT8_C(0xff)));
    CHECK(memcmp(&before, &io, sizeof(io)) == 0);

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4207), UINT8_C(0x34)));
    CHECK(io.htime == UINT16_C(0x0134));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4208), UINT8_C(0xfe)));
    CHECK(io.htime == UINT16_C(0x0034));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4208), UINT8_C(0xff)));
    CHECK(io.htime == UINT16_C(0x0134));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4209), UINT8_C(0xab)));
    CHECK(io.vtime == UINT16_C(0x01ab));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x420a), UINT8_C(0x02)));
    CHECK(io.vtime == UINT16_C(0x00ab));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x420a), UINT8_C(0x03)));
    CHECK(io.vtime == UINT16_C(0x01ab));

    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4207),
                                UINT8_C(0x6c), &value));
    CHECK(value == UINT8_C(0x6c));
    return 0;
}

static int test_nmi_gate_and_acknowledgement(void)
{
    mipsnes2_cpu_io io;
    uint8_t value;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    mipsnes2_cpu_io_set_vblank(&io, true);
    CHECK(io.vblank && !io.nmi_flag);
    mipsnes2_cpu_io_signal_nmi_flag(&io);
    CHECK(io.nmi_flag);
    CHECK(!mipsnes2_cpu_io_take_nmi_edge(&io));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0xff)));
    CHECK(io.nmitimen == UINT8_C(0xb1));
    CHECK(mipsnes2_cpu_io_irq_mode_get(&io) == MIPSNES2_CPU_IO_IRQ_HV);
    CHECK(mipsnes2_cpu_io_take_nmi_edge(&io));
    CHECK(!mipsnes2_cpu_io_take_nmi_edge(&io));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0xb1)));
    CHECK(!mipsnes2_cpu_io_take_nmi_edge(&io));

    mipsnes2_cpu_io_set_vblank(&io, true);
    CHECK(!mipsnes2_cpu_io_take_nmi_edge(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4210),
                                UINT8_C(0x70), &value));
    CHECK(value == UINT8_C(0xf2));
    CHECK(!io.nmi_flag);
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4210),
                                UINT8_C(0x50), &value));
    CHECK(value == UINT8_C(0x52));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), 0U));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0x80)));
    CHECK(!mipsnes2_cpu_io_take_nmi_edge(&io));
    mipsnes2_cpu_io_set_vblank(&io, false);
    CHECK(!io.vblank && !io.nmi_flag);
    mipsnes2_cpu_io_set_vblank(&io, true);
    CHECK(!io.nmi_flag && !mipsnes2_cpu_io_take_nmi_edge(&io));
    mipsnes2_cpu_io_signal_nmi_flag(&io);
    CHECK(io.nmi_flag && mipsnes2_cpu_io_take_nmi_edge(&io));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), 0U));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0x80)));
    CHECK(mipsnes2_cpu_io_take_nmi_edge(&io));
    mipsnes2_cpu_io_set_vblank(&io, false);
    CHECK(!io.nmi_flag);
    return 0;
}

static int test_irq_latch_and_acknowledgement(void)
{
    mipsnes2_cpu_io io;
    uint8_t value;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    CHECK(mipsnes2_cpu_io_irq_mode_get(NULL) ==
          MIPSNES2_CPU_IO_IRQ_DISABLED);
    CHECK(!mipsnes2_cpu_io_irq_asserted(NULL));
    mipsnes2_cpu_io_signal_timer_irq(NULL);
    mipsnes2_cpu_io_signal_timer_irq(&io);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&io));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0x10)));
    CHECK(mipsnes2_cpu_io_irq_mode_get(&io) == MIPSNES2_CPU_IO_IRQ_H);
    mipsnes2_cpu_io_signal_timer_irq(&io);
    CHECK(mipsnes2_cpu_io_irq_asserted(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4211),
                                UINT8_C(0x55), &value));
    CHECK(value == UINT8_C(0xd5));
    CHECK(!mipsnes2_cpu_io_irq_asserted(&io));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0x20)));
    CHECK(mipsnes2_cpu_io_irq_mode_get(&io) == MIPSNES2_CPU_IO_IRQ_V);
    mipsnes2_cpu_io_signal_timer_irq(&io);
    CHECK(mipsnes2_cpu_io_irq_asserted(&io));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0x80)));
    CHECK(mipsnes2_cpu_io_irq_mode_get(&io) ==
          MIPSNES2_CPU_IO_IRQ_DISABLED);
    CHECK(!mipsnes2_cpu_io_irq_asserted(&io));
    return 0;
}

static int test_hvbjoy_and_read_only_writes(void)
{
    mipsnes2_cpu_io io;
    mipsnes2_cpu_io before;
    uint8_t value;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    mipsnes2_cpu_io_set_vblank(NULL, true);
    mipsnes2_cpu_io_set_hblank(NULL, true);
    mipsnes2_cpu_io_signal_nmi_flag(NULL);
    mipsnes2_cpu_io_set_vblank(&io, true);
    mipsnes2_cpu_io_set_hblank(&io, true);
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(1)));
    CHECK(mipsnes2_cpu_io_begin_auto_joypad_read(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4212),
                                UINT8_C(0x3e), &value));
    CHECK(value == UINT8_C(0xff));

    before = io;
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4210), UINT8_C(0xff)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4211), UINT8_C(0xff)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4212), UINT8_C(0xff)));
    CHECK(memcmp(&before, &io, sizeof(io)) == 0);

    mipsnes2_cpu_io_set_hblank(&io, false);
    CHECK(mipsnes2_cpu_io_complete_auto_joypad_read(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4212),
                                UINT8_C(0x2a), &value));
    CHECK(value == UINT8_C(0xaa));
    return 0;
}

static int test_auto_joypad_snapshot_and_result_registers(void)
{
    static const uint16_t first_inputs[MIPSNES2_CPU_IO_JOYPAD_COUNT] = {
        UINT16_C(0x1234), UINT16_C(0xabcd),
        UINT16_C(0x00f0), UINT16_C(0x8001)
    };
    mipsnes2_cpu_io io;
    mipsnes2_cpu_io before;
    uint8_t value;
    uint8_t index;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    CHECK(!mipsnes2_cpu_io_auto_joypad_enabled(NULL));
    CHECK(!mipsnes2_cpu_io_auto_joypad_enabled(&io));
    CHECK(!mipsnes2_cpu_io_set_joypad_input(NULL, 0U, UINT16_C(1)));
    CHECK(!mipsnes2_cpu_io_set_joypad_input(
        &io, MIPSNES2_CPU_IO_JOYPAD_COUNT, UINT16_C(1)));
    CHECK(!mipsnes2_cpu_io_begin_auto_joypad_read(NULL));
    CHECK(!mipsnes2_cpu_io_begin_auto_joypad_read(&io));
    CHECK(!mipsnes2_cpu_io_complete_auto_joypad_read(NULL));
    CHECK(!mipsnes2_cpu_io_complete_auto_joypad_read(&io));

    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        CHECK(io.joypad_input[index] == 0U);
        CHECK(io.joypad_result[index] == 0U);
        CHECK(mipsnes2_cpu_io_set_joypad_input(
            &io, index, first_inputs[index]));
    }
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(0x01)));
    CHECK(mipsnes2_cpu_io_auto_joypad_enabled(&io));
    CHECK(mipsnes2_cpu_io_begin_auto_joypad_read(&io));
    CHECK(io.auto_joypad_busy);
    CHECK(!mipsnes2_cpu_io_begin_auto_joypad_read(&io));

    CHECK(mipsnes2_cpu_io_set_joypad_input(&io, 0U, UINT16_C(0xffff)));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4218),
                                UINT8_C(0xa5), &value));
    CHECK(value == 0U);
    CHECK(mipsnes2_cpu_io_complete_auto_joypad_read(&io));
    CHECK(!io.auto_joypad_busy);
    CHECK(!mipsnes2_cpu_io_complete_auto_joypad_read(&io));

    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        uint16_t offset = (uint16_t)(UINT16_C(0x4218) + index * 2U);

        CHECK(io.joypad_result[index] == first_inputs[index]);
        CHECK(mipsnes2_cpu_io_read(&io, offset, UINT8_C(0xa5), &value));
        CHECK(value == (uint8_t)first_inputs[index]);
        CHECK(mipsnes2_cpu_io_read(&io, (uint16_t)(offset + 1U),
                                    UINT8_C(0xa5), &value));
        CHECK(value == (uint8_t)(first_inputs[index] >> 8));
    }

    before = io;
    for (index = 0U; index < 8U; ++index) {
        CHECK(mipsnes2_cpu_io_write(
            &io, (uint16_t)(UINT16_C(0x4218) + index),
            (uint8_t)(UINT8_C(0x80) + index)));
    }
    CHECK(memcmp(&before, &io, sizeof(io)) == 0);

    CHECK(mipsnes2_cpu_io_begin_auto_joypad_read(&io));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), 0U));
    CHECK(!mipsnes2_cpu_io_auto_joypad_enabled(&io));
    CHECK(io.auto_joypad_busy);
    CHECK(mipsnes2_cpu_io_complete_auto_joypad_read(&io));
    CHECK(io.joypad_result[0] == UINT16_MAX);
    return 0;
}

static int latch_manual_inputs(mipsnes2_cpu_io *io)
{
    return mipsnes2_cpu_io_write(io, UINT16_C(0x4016), UINT8_C(1)) &&
           mipsnes2_cpu_io_write(io, UINT16_C(0x4016), 0U)
               ? 0
               : 1;
}

static int read_manual_pair(mipsnes2_cpu_io *io,
                            uint16_t offset,
                            uint16_t *first,
                            uint16_t *second)
{
    uint16_t first_result = 0U;
    uint16_t second_result = 0U;
    uint8_t bit;

    for (bit = 0U; bit < 16U; ++bit) {
        uint8_t value;

        if (!mipsnes2_cpu_io_read(io, offset, UINT8_C(0xa0), &value)) {
            return 1;
        }
        first_result = (uint16_t)((first_result << 1) | (value & 1U));
        second_result =
            (uint16_t)((second_result << 1) | ((value >> 1) & 1U));
    }
    *first = first_result;
    *second = second_result;
    return 0;
}

static int test_manual_joypad_ports(void)
{
    static const uint16_t inputs[MIPSNES2_CPU_IO_JOYPAD_COUNT] = {
        UINT16_C(0xa55a), UINT16_C(0x5aa5),
        UINT16_C(0x8001), UINT16_C(0x7ffe)
    };
    mipsnes2_cpu_io io;
    uint16_t first;
    uint16_t second;
    uint8_t value;
    uint8_t index;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    CHECK(!mipsnes2_cpu_io_set_joypad_connected(NULL, 0U, true));
    CHECK(!mipsnes2_cpu_io_set_joypad_connected(
        &io, MIPSNES2_CPU_IO_JOYPAD_COUNT, true));
    for (index = 0U; index < MIPSNES2_CPU_IO_JOYPAD_COUNT; ++index) {
        CHECK(mipsnes2_cpu_io_set_joypad_input(&io, index, inputs[index]));
    }
    CHECK(mipsnes2_cpu_io_set_joypad_connected(&io, 0U, true));
    CHECK(mipsnes2_cpu_io_set_joypad_connected(&io, 1U, false));
    CHECK(mipsnes2_cpu_io_set_joypad_connected(&io, 2U, false));
    CHECK(mipsnes2_cpu_io_set_joypad_connected(&io, 3U, true));
    CHECK(io.joypad_connected_mask == UINT8_C(0x09));

    CHECK(!mipsnes2_cpu_io_write(&io, UINT16_C(0x4017), UINT8_C(1)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4016), UINT8_C(0xff)));
    CHECK(io.joypad_strobe && io.joypad_output == UINT8_C(7));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4016),
                                UINT8_C(0xa4), &value));
    CHECK(value == UINT8_C(0xa7));
    CHECK(io.manual_joypad_index[0] == 0U);
    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &io, 0U, UINT16_C(0x255a)));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4016),
                                UINT8_C(0xa4), &value));
    CHECK(value == UINT8_C(0xa6));
    CHECK(io.manual_joypad_index[0] == 0U);

    CHECK(mipsnes2_cpu_io_set_joypad_input(&io, 0U, inputs[0]));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4016), UINT8_C(6)));
    CHECK(!io.joypad_strobe && io.joypad_output == UINT8_C(6));
    CHECK(io.manual_joypad_connected_mask == UINT8_C(0x09));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4016),
                                UINT8_C(0xa4), &value));
    CHECK(value == UINT8_C(0xa7));
    CHECK(io.manual_joypad_index[0] == UINT8_C(1));
    CHECK(io.manual_joypad_index[1] == 0U);
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4017),
                                UINT8_C(0xa0), &value));
    CHECK(value == UINT8_C(0xbc));
    CHECK(io.manual_joypad_index[1] == UINT8_C(1));

    CHECK(latch_manual_inputs(&io) == 0);
    CHECK(mipsnes2_cpu_io_set_joypad_input(&io, 0U, 0U));
    CHECK(read_manual_pair(&io, UINT16_C(0x4016), &first, &second) == 0);
    CHECK(first == inputs[0] && second == inputs[2]);
    CHECK(io.manual_joypad_index[0] == UINT8_C(16));
    CHECK(io.manual_joypad_index[1] == 0U);
    CHECK(read_manual_pair(&io, UINT16_C(0x4017), &first, &second) == 0);
    CHECK(first == inputs[1] && second == inputs[3]);
    CHECK(io.manual_joypad_index[1] == UINT8_C(16));

    CHECK(mipsnes2_cpu_io_set_joypad_connected(&io, 0U, false));
    CHECK(mipsnes2_cpu_io_set_joypad_connected(&io, 2U, true));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4016),
                                UINT8_C(0x54), &value));
    CHECK(value == UINT8_C(0x55));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4017),
                                UINT8_C(0x60), &value));
    CHECK(value == UINT8_C(0x7e));
    CHECK(io.manual_joypad_index[0] == UINT8_C(16));
    CHECK(io.manual_joypad_index[1] == UINT8_C(16));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4200), UINT8_C(1)));
    CHECK(mipsnes2_cpu_io_begin_auto_joypad_read(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4016), 0U, &value));
    CHECK(mipsnes2_cpu_io_complete_auto_joypad_read(&io));
    CHECK(io.joypad_result[0] == 0U);
    return 0;
}

static int test_programmable_io_port(void)
{
    mipsnes2_cpu_io io;
    mipsnes2_cpu_io before;
    uint8_t value;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    CHECK(io.wrio == UINT8_MAX && io.io_port_input == UINT8_MAX);
    CHECK(!mipsnes2_cpu_io_set_io_port_input(NULL, 0U));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(NULL));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4201),
                                UINT8_C(0x5a), &value));
    CHECK(value == UINT8_C(0x5a));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4213), 0U, &value));
    CHECK(value == UINT8_MAX);

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4201), UINT8_C(0x7f)));
    CHECK(io.wrio == UINT8_C(0x7f));
    CHECK(mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4201), UINT8_C(0x7f)));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4201), UINT8_MAX));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(&io));

    CHECK(mipsnes2_cpu_io_set_io_port_input(&io, UINT8_C(0xbf)));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4213), 0U, &value));
    CHECK(value == UINT8_C(0xbf));
    CHECK(mipsnes2_cpu_io_set_io_port_input(&io, UINT8_C(0x35)));
    CHECK(mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4213), 0U, &value));
    CHECK(value == UINT8_C(0x35));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4201), UINT8_C(0xaa)));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(mipsnes2_cpu_io_set_io_port_input(&io, UINT8_MAX));
    CHECK(!mipsnes2_cpu_io_take_hv_latch_edge(&io));
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4213), 0U, &value));
    CHECK(value == UINT8_C(0xaa));

    before = io;
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4213), UINT8_C(0x11)));
    CHECK(memcmp(&before, &io, sizeof(io)) == 0);
    return 0;
}

static int read_math(mipsnes2_cpu_io *io, uint16_t offset,
                     uint8_t *value)
{
    return mipsnes2_cpu_io_read(io, offset, UINT8_C(0xa5), value) ? 0 : 1;
}

static int test_math_registers_and_cycle_boundaries(void)
{
    mipsnes2_cpu_io io;
    mipsnes2_cpu_io before;
    uint8_t value;

    mipsnes2_cpu_io_init(&io, UINT8_C(2));
    CHECK(!mipsnes2_cpu_io_math_busy(NULL));
    mipsnes2_cpu_io_advance_math_cycles(NULL, UINT64_MAX);
    CHECK(!mipsnes2_cpu_io_math_busy(&io));
    CHECK(io.rddiv == 0U && io.rdmpy == 0U);

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4202), UINT8_C(0x2a)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4203), UINT8_C(0x81)));
    CHECK(io.wrmpya == UINT8_C(0x2a));
    CHECK(io.wrmpyb == UINT8_C(0x81));
    CHECK(io.math_operation == MIPSNES2_CPU_IO_MATH_MULTIPLY);
    CHECK(io.math_cycles_remaining == UINT8_C(8));
    CHECK(mipsnes2_cpu_io_math_busy(&io));
    CHECK(read_math(&io, UINT16_C(0x4216), &value) == 0 && value == 0U);
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_C(7));
    CHECK(io.math_cycles_remaining == UINT8_C(1));
    CHECK(io.rddiv == 0U && io.rdmpy == 0U);
    mipsnes2_cpu_io_advance_math_cycles(&io, 0U);
    CHECK(io.math_cycles_remaining == UINT8_C(1));
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_C(1));
    CHECK(!mipsnes2_cpu_io_math_busy(&io));
    CHECK(io.math_operation == MIPSNES2_CPU_IO_MATH_IDLE);
    CHECK(io.rddiv == UINT16_C(0x0081));
    CHECK(io.rdmpy == UINT16_C(0x152a));
    CHECK(read_math(&io, UINT16_C(0x4214), &value) == 0 &&
          value == UINT8_C(0x81));
    CHECK(read_math(&io, UINT16_C(0x4215), &value) == 0 && value == 0U);
    CHECK(read_math(&io, UINT16_C(0x4216), &value) == 0 &&
          value == UINT8_C(0x2a));
    CHECK(read_math(&io, UINT16_C(0x4217), &value) == 0 &&
          value == UINT8_C(0x15));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4204), UINT8_C(0x34)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4205), UINT8_C(0x12)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4206), UINT8_C(0x11)));
    CHECK(io.wrdiv == UINT16_C(0x1234));
    CHECK(io.wrdivb == UINT8_C(0x11));
    CHECK(io.math_operation == MIPSNES2_CPU_IO_MATH_DIVIDE);
    CHECK(io.math_cycles_remaining == UINT8_C(16));
    CHECK(io.rddiv == UINT16_C(0x0081) && io.rdmpy == UINT16_C(0x152a));
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_C(15));
    CHECK(io.math_cycles_remaining == UINT8_C(1));
    CHECK(io.rddiv == UINT16_C(0x0081) && io.rdmpy == UINT16_C(0x152a));
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_MAX);
    CHECK(io.rddiv == UINT16_C(0x0112));
    CHECK(io.rdmpy == UINT16_C(0x0002));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4206), 0U));
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_C(16));
    CHECK(io.rddiv == UINT16_MAX && io.rdmpy == UINT16_C(0x1234));

    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4202), UINT8_C(9)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4203), UINT8_C(9)));
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_C(3));
    CHECK(io.math_cycles_remaining == UINT8_C(5));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4202), UINT8_C(4)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4203), UINT8_C(5)));
    CHECK(io.math_cycles_remaining == UINT8_C(8));
    mipsnes2_cpu_io_advance_math_cycles(&io, UINT64_C(8));
    CHECK(io.rddiv == UINT16_C(5) && io.rdmpy == UINT16_C(20));

    before = io;
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4214), UINT8_C(0xaa)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4215), UINT8_C(0xbb)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4216), UINT8_C(0xcc)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4217), UINT8_C(0xdd)));
    CHECK(memcmp(&before, &io, sizeof(io)) == 0);
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4202),
                                UINT8_C(0x6c), &value));
    CHECK(value == UINT8_C(0x6c));
    return 0;
}

int main(void)
{
    CHECK(test_initialization_and_timer_registers() == 0);
    CHECK(test_nmi_gate_and_acknowledgement() == 0);
    CHECK(test_irq_latch_and_acknowledgement() == 0);
    CHECK(test_hvbjoy_and_read_only_writes() == 0);
    CHECK(test_auto_joypad_snapshot_and_result_registers() == 0);
    CHECK(test_manual_joypad_ports() == 0);
    CHECK(test_programmable_io_port() == 0);
    CHECK(test_math_registers_and_cycle_boundaries() == 0);
    (void)puts("PASS: Ricoh 5A22 CPU-I/O, math, joypad and interrupt foundation");
    return 0;
}
