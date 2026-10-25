/* MIPSNES2 S-CPU/S-SMP communication ports. See LICENSE for GPL-3.0 terms. */
#include "core/apu/io_ports.h"

#include <string.h>

static bool cpu_port_index(uint16_t address, unsigned *port)
{
    if (port == NULL || address < UINT16_C(0x2140) ||
        address > UINT16_C(0x217f)) {
        return false;
    }
    *port = address & 3U;
    return true;
}

void mipsnes2_apu_io_ports_init(mipsnes2_apu_io_ports *ports)
{
    if (ports != NULL) {
        (void)memset(ports, 0, sizeof(*ports));
        ports->initialized = true;
    }
}

bool mipsnes2_apu_io_ports_cpu_read(const mipsnes2_apu_io_ports *ports,
                                    uint16_t address, uint8_t *value)
{
    unsigned port;

    if (ports == NULL || !ports->initialized || value == NULL ||
        !cpu_port_index(address, &port)) {
        return false;
    }
    *value = ports->apu_to_cpu[port];
    return true;
}

bool mipsnes2_apu_io_ports_cpu_write(mipsnes2_apu_io_ports *ports,
                                     uint16_t address, uint8_t value)
{
    unsigned port;

    if (ports == NULL || !ports->initialized ||
        !cpu_port_index(address, &port)) {
        return false;
    }
    ports->cpu_to_apu[port] = value;
    return true;
}

bool mipsnes2_apu_io_ports_smp_read(const mipsnes2_apu_io_ports *ports,
                                    unsigned port, uint8_t *value)
{
    if (ports == NULL || !ports->initialized || value == NULL ||
        port >= MIPSNES2_APU_IO_PORT_COUNT) {
        return false;
    }
    *value = ports->cpu_to_apu[port];
    return true;
}

bool mipsnes2_apu_io_ports_smp_write(mipsnes2_apu_io_ports *ports,
                                     unsigned port, uint8_t value)
{
    if (ports == NULL || !ports->initialized ||
        port >= MIPSNES2_APU_IO_PORT_COUNT) {
        return false;
    }
    ports->apu_to_cpu[port] = value;
    return true;
}

bool mipsnes2_apu_io_ports_smp_clear_inputs(mipsnes2_apu_io_ports *ports,
                                            bool ports_01, bool ports_23)
{
    if (ports == NULL || !ports->initialized) {
        return false;
    }
    if (ports_01) {
        ports->cpu_to_apu[0] = 0U;
        ports->cpu_to_apu[1] = 0U;
    }
    if (ports_23) {
        ports->cpu_to_apu[2] = 0U;
        ports->cpu_to_apu[3] = 0U;
    }
    return true;
}

static uint8_t cpu_read8(void *context, mipsnes2_address address)
{
    mipsnes2_apu_io_ports *ports = context;
    uint8_t value = 0U;

    (void)mipsnes2_apu_io_ports_cpu_read(ports, (uint16_t)address, &value);
    return value;
}

static void cpu_write8(void *context, mipsnes2_address address, uint8_t value)
{
    (void)mipsnes2_apu_io_ports_cpu_write(
        context, (uint16_t)address, value);
}

mipsnes2_bus_interface mipsnes2_apu_io_ports_make_cpu_interface(
    mipsnes2_apu_io_ports *ports)
{
    mipsnes2_bus_interface interface;

    interface.context = ports;
    interface.read8 = cpu_read8;
    interface.write8 = cpu_write8;
    return interface;
}
