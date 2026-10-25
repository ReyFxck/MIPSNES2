/* Deterministic S-CPU/S-SMP communication-port fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/io_ports.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

int main(void)
{
    mipsnes2_apu_io_ports ports;
    mipsnes2_bus_interface cpu;
    uint8_t value = UINT8_C(0xff);
    unsigned port;

    (void)memset(&ports, UINT8_C(0xa5), sizeof(ports));
    mipsnes2_apu_io_ports_init(&ports);
    CHECK(ports.initialized);
    for (port = 0U; port < MIPSNES2_APU_IO_PORT_COUNT; ++port) {
        CHECK(ports.cpu_to_apu[port] == 0U && ports.apu_to_cpu[port] == 0U);
    }

    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2140),
                                          UINT8_C(0x10)));
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2145),
                                          UINT8_C(0x21)));
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x217e),
                                          UINT8_C(0x32)));
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x217f),
                                          UINT8_C(0x43)));
    for (port = 0U; port < MIPSNES2_APU_IO_PORT_COUNT; ++port) {
        CHECK(mipsnes2_apu_io_ports_smp_read(&ports, port, &value));
        CHECK(value == (uint8_t)(UINT8_C(0x10) + port * UINT8_C(0x11)));
    }

    for (port = 0U; port < MIPSNES2_APU_IO_PORT_COUNT; ++port) {
        CHECK(mipsnes2_apu_io_ports_smp_write(
            &ports, port, (uint8_t)(UINT8_C(0x84) + port)));
        CHECK(mipsnes2_apu_io_ports_cpu_read(
            &ports, (uint16_t)(UINT16_C(0x2160) + port), &value));
        CHECK(value == (uint8_t)(UINT8_C(0x84) + port));
    }
    CHECK(ports.cpu_to_apu[0] == UINT8_C(0x10));
    CHECK(ports.apu_to_cpu[0] == UINT8_C(0x84));

    cpu = mipsnes2_apu_io_ports_make_cpu_interface(&ports);
    cpu.write8(cpu.context, UINT32_C(0x002174), UINT8_C(0x9a));
    CHECK(ports.cpu_to_apu[0] == UINT8_C(0x9a));
    CHECK(cpu.read8(cpu.context, UINT32_C(0x802177)) == UINT8_C(0x87));

    CHECK(!mipsnes2_apu_io_ports_cpu_read(NULL, UINT16_C(0x2140), &value));
    CHECK(!mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x213f), &value));
    CHECK(!mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140), NULL));
    CHECK(!mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2180), 0U));
    CHECK(!mipsnes2_apu_io_ports_smp_read(&ports, 4U, &value));
    CHECK(!mipsnes2_apu_io_ports_smp_write(&ports, 4U, 0U));

    (void)puts("APU communication ports: OK");
    return 0;
}
