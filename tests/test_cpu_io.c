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
    CHECK(!mipsnes2_cpu_io_read(&io, UINT16_C(0x4213), 0U, &value));
    CHECK(value == UINT8_C(0x5a));
    CHECK(!mipsnes2_cpu_io_write(&io, UINT16_C(0x4201), UINT8_C(0xff)));
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
    mipsnes2_cpu_io_set_auto_joypad_busy(NULL, true);
    mipsnes2_cpu_io_signal_nmi_flag(NULL);
    mipsnes2_cpu_io_set_vblank(&io, true);
    mipsnes2_cpu_io_set_hblank(&io, true);
    mipsnes2_cpu_io_set_auto_joypad_busy(&io, true);
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4212),
                                UINT8_C(0x3e), &value));
    CHECK(value == UINT8_C(0xff));

    before = io;
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4210), UINT8_C(0xff)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4211), UINT8_C(0xff)));
    CHECK(mipsnes2_cpu_io_write(&io, UINT16_C(0x4212), UINT8_C(0xff)));
    CHECK(memcmp(&before, &io, sizeof(io)) == 0);

    mipsnes2_cpu_io_set_hblank(&io, false);
    mipsnes2_cpu_io_set_auto_joypad_busy(&io, false);
    CHECK(mipsnes2_cpu_io_read(&io, UINT16_C(0x4212),
                                UINT8_C(0x2a), &value));
    CHECK(value == UINT8_C(0xaa));
    return 0;
}

int main(void)
{
    CHECK(test_initialization_and_timer_registers() == 0);
    CHECK(test_nmi_gate_and_acknowledgement() == 0);
    CHECK(test_irq_latch_and_acknowledgement() == 0);
    CHECK(test_hvbjoy_and_read_only_writes() == 0);
    (void)puts("PASS: Ricoh 5A22 CPU-I/O register and interrupt-latch foundation");
    return 0;
}
