/* MIPSNES2 logical bus foundation. See LICENSE for GPL-3.0 terms. */
#include "core/bus/bus.h"

#include <string.h>

#define MIPSNES2_LAST_ADDRESS UINT32_C(0x00ffffff)

static bool range_is_valid(mipsnes2_address first, size_t length,
                           mipsnes2_address *last)
{
    uint64_t end;

    if (first > MIPSNES2_LAST_ADDRESS || length == 0U) {
        return false;
    }
    end = (uint64_t)first + (uint64_t)length - UINT64_C(1);
    if (end > MIPSNES2_LAST_ADDRESS) {
        return false;
    }
    *last = (mipsnes2_address)end;
    return true;
}

static bool ranges_overlap(const mipsnes2_bus_region *region,
                           mipsnes2_address first, mipsnes2_address last)
{
    return first <= region->last && region->first <= last;
}

static mipsnes2_bus_result reserve_region(mipsnes2_bus *bus,
                                          mipsnes2_address first,
                                          size_t length,
                                          mipsnes2_bus_region **region)
{
    mipsnes2_address last;
    size_t index;

    if (bus == NULL || region == NULL) {
        return MIPSNES2_BUS_INVALID_ARGUMENT;
    }
    if (!range_is_valid(first, length, &last)) {
        return MIPSNES2_BUS_OUT_OF_RANGE;
    }
    for (index = 0; index < bus->region_count; ++index) {
        if (ranges_overlap(&bus->regions[index], first, last)) {
            return MIPSNES2_BUS_OVERLAP;
        }
    }
    if (bus->region_count == MIPSNES2_BUS_MAX_REGIONS) {
        return MIPSNES2_BUS_REGION_LIMIT;
    }
    *region = &bus->regions[bus->region_count++];
    (*region)->first = first;
    (*region)->last = last;
    return MIPSNES2_BUS_OK;
}

static mipsnes2_bus_region *find_region(mipsnes2_bus *bus,
                                        mipsnes2_address address)
{
    size_t index;

    for (index = 0; index < bus->region_count; ++index) {
        mipsnes2_bus_region *region = &bus->regions[index];
        if (address >= region->first && address <= region->last) {
            return region;
        }
    }
    return NULL;
}

void mipsnes2_bus_init(mipsnes2_bus *bus, uint8_t initial_open_value)
{
    if (bus == NULL) {
        return;
    }
    (void)memset(bus, 0, sizeof(*bus));
    bus->open_value = initial_open_value;
}

mipsnes2_bus_result mipsnes2_bus_map_direct(mipsnes2_bus *bus,
                                            mipsnes2_address first,
                                            size_t length,
                                            uint8_t *bytes,
                                            bool writable)
{
    mipsnes2_bus_region *region;
    mipsnes2_bus_result result;

    if (bytes == NULL) {
        return MIPSNES2_BUS_INVALID_ARGUMENT;
    }
    result = reserve_region(bus, first, length, &region);
    if (result != MIPSNES2_BUS_OK) {
        return result;
    }
    region->kind = MIPSNES2_BUS_REGION_DIRECT;
    region->target.direct.bytes = bytes;
    region->target.direct.writable = writable;
    return MIPSNES2_BUS_OK;
}

mipsnes2_bus_result mipsnes2_bus_map_mmio(mipsnes2_bus *bus,
                                          mipsnes2_address first,
                                          size_t length,
                                          mipsnes2_bus_read8 read8,
                                          mipsnes2_bus_write8 write8,
                                          void *context)
{
    mipsnes2_bus_region *region;
    mipsnes2_bus_result result;

    if (read8 == NULL && write8 == NULL) {
        return MIPSNES2_BUS_INVALID_ARGUMENT;
    }
    result = reserve_region(bus, first, length, &region);
    if (result != MIPSNES2_BUS_OK) {
        return result;
    }
    region->kind = MIPSNES2_BUS_REGION_MMIO;
    region->target.mmio.context = context;
    region->target.mmio.read8 = read8;
    region->target.mmio.write8 = write8;
    return MIPSNES2_BUS_OK;
}

uint8_t mipsnes2_bus_read(mipsnes2_bus *bus, mipsnes2_address address)
{
    mipsnes2_bus_region *region;
    uint8_t value;

    if (bus == NULL) {
        return 0U;
    }
    address = mipsnes2_address24(address);
    region = find_region(bus, address);
    if (region == NULL) {
        return bus->open_value;
    }
    if (region->kind == MIPSNES2_BUS_REGION_DIRECT) {
        value = region->target.direct.bytes[address - region->first];
    } else if (region->target.mmio.read8 != NULL) {
        value = region->target.mmio.read8(region->target.mmio.context,
                                          address);
    } else {
        value = bus->open_value;
    }
    bus->open_value = value;
    return value;
}

void mipsnes2_bus_write(mipsnes2_bus *bus, mipsnes2_address address,
                        uint8_t value)
{
    mipsnes2_bus_region *region;

    if (bus == NULL) {
        return;
    }
    address = mipsnes2_address24(address);
    region = find_region(bus, address);
    if (region != NULL) {
        if (region->kind == MIPSNES2_BUS_REGION_DIRECT) {
            if (region->target.direct.writable) {
                region->target.direct.bytes[address - region->first] = value;
            }
        } else if (region->target.mmio.write8 != NULL) {
            region->target.mmio.write8(region->target.mmio.context, address,
                                       value);
        }
    }
    bus->open_value = value;
}

static uint8_t interface_read(void *context, mipsnes2_address address)
{
    return mipsnes2_bus_read((mipsnes2_bus *)context, address);
}

static void interface_write(void *context, mipsnes2_address address,
                            uint8_t value)
{
    mipsnes2_bus_write((mipsnes2_bus *)context, address, value);
}

mipsnes2_bus_interface mipsnes2_bus_make_interface(mipsnes2_bus *bus)
{
    mipsnes2_bus_interface interface;

    interface.context = bus;
    interface.read8 = bus != NULL ? interface_read : NULL;
    interface.write8 = bus != NULL ? interface_write : NULL;
    return interface;
}
