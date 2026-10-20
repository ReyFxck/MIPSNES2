/* MIPSNES2 SNES logical bus foundation. See LICENSE for GPL-3.0 terms. */
#include "core/bus/snes_bus.h"

#include <limits.h>
#include <string.h>

#include "core/scheduler/5a22_timing.h"

static bool is_system_bank(uint8_t bank)
{
    return bank <= UINT8_C(0x3f) ||
           (bank >= UINT8_C(0x80) && bank <= UINT8_C(0xbf));
}

static bool add_master_clocks(mipsnes2_snes_bus *bus, uint64_t clocks)
{
    if (bus->master_clocks > UINT64_MAX - clocks) {
        bus->timing_overflow = true;
        return false;
    }
    bus->master_clocks += clocks;
    return true;
}

static void account_access(mipsnes2_snes_bus *bus,
                           mipsnes2_address address)
{
    uint8_t clocks = mipsnes2_5a22_access_master_clocks(
        address, bus->fastrom_enabled);

    if (bus->access_count == UINT64_MAX) {
        bus->timing_overflow = true;
    } else {
        bus->access_count++;
    }
    (void)add_master_clocks(bus, clocks);
}

static uint8_t routed_read(mipsnes2_bus_interface route,
                           mipsnes2_address address,
                           uint8_t open_value)
{
    if (route.read8 == NULL) {
        return open_value;
    }
    return route.read8(route.context, address);
}

static void routed_write(mipsnes2_bus_interface route,
                         mipsnes2_address address,
                         uint8_t value)
{
    if (route.write8 != NULL) {
        route.write8(route.context, address, value);
    }
}

static uint8_t cartridge_read(mipsnes2_snes_bus *bus,
                              mipsnes2_address address)
{
    uint8_t value;

    if (bus->cartridge.read8 == NULL ||
        !bus->cartridge.read8(bus->cartridge.context, address, &value)) {
        return bus->open_value;
    }
    return value;
}

static void cartridge_write(mipsnes2_snes_bus *bus,
                            mipsnes2_address address,
                            uint8_t value)
{
    if (bus->cartridge.write8 != NULL) {
        (void)bus->cartridge.write8(bus->cartridge.context, address, value);
    }
}

static bool wram_index(mipsnes2_address address, uint32_t *index)
{
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t offset = (uint16_t)address;

    if (bank == UINT8_C(0x7e) || bank == UINT8_C(0x7f)) {
        *index = ((uint32_t)(bank - UINT8_C(0x7e)) << 16) | offset;
        return true;
    }
    if (is_system_bank(bank) && offset < UINT16_C(0x2000)) {
        *index = offset;
        return true;
    }
    return false;
}

static uint8_t read_wram_port(mipsnes2_snes_bus *bus, uint16_t offset)
{
    uint8_t value;

    if (offset != UINT16_C(0x2180)) {
        return bus->open_value;
    }
    value = bus->wram[bus->wram_port_address];
    bus->wram_port_address =
        (bus->wram_port_address + UINT32_C(1)) & UINT32_C(0x1ffff);
    return value;
}

static void write_wram_port(mipsnes2_snes_bus *bus,
                            uint16_t offset,
                            uint8_t value)
{
    switch (offset) {
    case 0x2180:
        bus->wram[bus->wram_port_address] = value;
        bus->wram_port_address =
            (bus->wram_port_address + UINT32_C(1)) & UINT32_C(0x1ffff);
        break;
    case 0x2181:
        bus->wram_port_address =
            (bus->wram_port_address & UINT32_C(0x1ff00)) | value;
        break;
    case 0x2182:
        bus->wram_port_address =
            (bus->wram_port_address & UINT32_C(0x100ff)) |
            ((uint32_t)value << 8);
        break;
    case 0x2183:
        bus->wram_port_address =
            (bus->wram_port_address & UINT32_C(0x0ffff)) |
            (((uint32_t)value & UINT32_C(1)) << 16);
        break;
    default:
        break;
    }
}

static uint8_t read_system_area(mipsnes2_snes_bus *bus,
                                mipsnes2_address address,
                                uint16_t offset)
{
    uint8_t value;

    if (offset >= UINT16_C(0x2100) && offset <= UINT16_C(0x213f)) {
        return routed_read(bus->ppu, address, bus->open_value);
    }
    if (offset >= UINT16_C(0x2140) && offset <= UINT16_C(0x217f)) {
        return routed_read(bus->apu, address, bus->open_value);
    }
    if (offset >= UINT16_C(0x2180) && offset <= UINT16_C(0x2183)) {
        return read_wram_port(bus, offset);
    }
    if (offset >= UINT16_C(0x4000) && offset <= UINT16_C(0x421f)) {
        if (offset == UINT16_C(0x420d)) {
            return bus->open_value;
        }
        if (bus->cpu_io_state != NULL &&
            mipsnes2_cpu_io_read(bus->cpu_io_state, offset,
                                 bus->open_value, &value)) {
            return value;
        }
        return routed_read(bus->cpu_io, address, bus->open_value);
    }
    if (offset >= UINT16_C(0x4300) && offset <= UINT16_C(0x437f)) {
        return routed_read(bus->dma, address, bus->open_value);
    }
    if ((offset >= UINT16_C(0x2100) && offset <= UINT16_C(0x21ff)) ||
        (offset >= UINT16_C(0x4200) && offset <= UINT16_C(0x43ff))) {
        return bus->open_value;
    }
    return cartridge_read(bus, address);
}

static void write_system_area(mipsnes2_snes_bus *bus,
                              mipsnes2_address address,
                              uint16_t offset,
                              uint8_t value)
{
    if (offset >= UINT16_C(0x2100) && offset <= UINT16_C(0x213f)) {
        routed_write(bus->ppu, address, value);
        return;
    }
    if (offset >= UINT16_C(0x2140) && offset <= UINT16_C(0x217f)) {
        routed_write(bus->apu, address, value);
        return;
    }
    if (offset >= UINT16_C(0x2180) && offset <= UINT16_C(0x2183)) {
        write_wram_port(bus, offset, value);
        return;
    }
    if (offset >= UINT16_C(0x4000) && offset <= UINT16_C(0x421f)) {
        if (offset == UINT16_C(0x420d)) {
            bus->fastrom_enabled = (value & UINT8_C(1)) != 0U;
            return;
        }
        if (bus->cpu_io_state != NULL &&
            mipsnes2_cpu_io_write(bus->cpu_io_state, offset, value)) {
            return;
        }
        routed_write(bus->cpu_io, address, value);
        return;
    }
    if (offset >= UINT16_C(0x4300) && offset <= UINT16_C(0x437f)) {
        routed_write(bus->dma, address, value);
        return;
    }
    if ((offset >= UINT16_C(0x2100) && offset <= UINT16_C(0x21ff)) ||
        (offset >= UINT16_C(0x4200) && offset <= UINT16_C(0x43ff))) {
        return;
    }
    cartridge_write(bus, address, value);
}

mipsnes2_snes_bus_result mipsnes2_snes_bus_init(
    mipsnes2_snes_bus *bus,
    const mipsnes2_snes_bus_config *config)
{
    if (bus == NULL || config == NULL || config->wram == NULL ||
        config->wram_size < MIPSNES2_SNES_WRAM_SIZE) {
        return MIPSNES2_SNES_BUS_INVALID_ARGUMENT;
    }

    (void)memset(bus, 0, sizeof(*bus));
    bus->wram = config->wram;
    bus->ppu = config->ppu;
    bus->apu = config->apu;
    bus->cpu_io = config->cpu_io;
    bus->cpu_io_state = config->cpu_io_state;
    bus->dma = config->dma;
    bus->cartridge = config->cartridge;
    bus->open_value = config->initial_open_value;
    return MIPSNES2_SNES_BUS_OK;
}

uint8_t mipsnes2_snes_bus_read(mipsnes2_snes_bus *bus,
                               mipsnes2_address address)
{
    uint32_t index;
    uint8_t bank;
    uint16_t offset;
    uint8_t value;

    if (bus == NULL || bus->wram == NULL) {
        return 0U;
    }
    address = mipsnes2_address24(address);
    account_access(bus, address);
    if (wram_index(address, &index)) {
        value = bus->wram[index];
    } else {
        bank = (uint8_t)(address >> 16);
        offset = (uint16_t)address;
        value = is_system_bank(bank)
                    ? read_system_area(bus, address, offset)
                    : cartridge_read(bus, address);
    }
    mipsnes2_cpu_io_advance_math_cycles(bus->cpu_io_state, UINT64_C(1));
    bus->open_value = value;
    return value;
}

void mipsnes2_snes_bus_write(mipsnes2_snes_bus *bus,
                             mipsnes2_address address,
                             uint8_t value)
{
    uint32_t index;
    uint8_t bank;

    if (bus == NULL || bus->wram == NULL) {
        return;
    }
    address = mipsnes2_address24(address);
    account_access(bus, address);
    mipsnes2_cpu_io_advance_math_cycles(bus->cpu_io_state, UINT64_C(1));
    if (wram_index(address, &index)) {
        bus->wram[index] = value;
    } else {
        bank = (uint8_t)(address >> 16);
        if (is_system_bank(bank)) {
            write_system_area(bus, address, (uint16_t)address, value);
        } else {
            cartridge_write(bus, address, value);
        }
    }
    bus->open_value = value;
}

mipsnes2_snes_bus_result mipsnes2_snes_bus_account_internal_cycles(
    mipsnes2_snes_bus *bus,
    uint64_t processor_cycles)
{
    uint64_t clocks;

    if (bus == NULL || bus->wram == NULL) {
        return MIPSNES2_SNES_BUS_INVALID_ARGUMENT;
    }
    if (processor_cycles > UINT64_MAX /
                               MIPSNES2_5A22_INTERNAL_MASTER_CLOCKS) {
        bus->timing_overflow = true;
        return MIPSNES2_SNES_BUS_TIMING_OVERFLOW;
    }
    clocks = processor_cycles * MIPSNES2_5A22_INTERNAL_MASTER_CLOCKS;
    if (!add_master_clocks(bus, clocks)) {
        return MIPSNES2_SNES_BUS_TIMING_OVERFLOW;
    }
    mipsnes2_cpu_io_advance_math_cycles(bus->cpu_io_state,
                                        processor_cycles);
    return MIPSNES2_SNES_BUS_OK;
}

static uint8_t interface_read(void *context, mipsnes2_address address)
{
    return mipsnes2_snes_bus_read((mipsnes2_snes_bus *)context, address);
}

static void interface_write(void *context,
                            mipsnes2_address address,
                            uint8_t value)
{
    mipsnes2_snes_bus_write((mipsnes2_snes_bus *)context, address, value);
}

mipsnes2_bus_interface mipsnes2_snes_bus_make_interface(
    mipsnes2_snes_bus *bus)
{
    mipsnes2_bus_interface interface;

    interface.context = bus;
    interface.read8 = bus != NULL ? interface_read : NULL;
    interface.write8 = bus != NULL ? interface_write : NULL;
    return interface;
}

bool mipsnes2_snes_bus_interface_matches(
    const mipsnes2_snes_bus *bus,
    mipsnes2_bus_interface interface)
{
    return bus != NULL && interface.context == bus &&
           interface.read8 == interface_read &&
           interface.write8 == interface_write;
}
