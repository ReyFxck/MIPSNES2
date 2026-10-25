/* MIPSNES2 S-SMP local memory map. See LICENSE for GPL-3.0 terms. */
#include "core/apu/spc700/memory.h"

bool mipsnes2_smp_memory_init(mipsnes2_smp_memory *memory,
                              uint8_t *ram, size_t ram_size,
                              mipsnes2_apu_io_ports *ports,
                              const uint8_t *ipl, size_t ipl_size)
{
    if (memory == NULL) {
        return false;
    }
    memory->initialized = false;
    if (ram == NULL || ram_size < MIPSNES2_SMP_RAM_SIZE ||
        ports == NULL || !ports->initialized ||
        ((ipl == NULL && ipl_size != 0U) ||
         (ipl != NULL && ipl_size != MIPSNES2_SMP_IPL_SIZE))) {
        return false;
    }
    memory->initialized = true;
    memory->ram = ram;
    memory->ports = ports;
    memory->ipl = ipl;
    mipsnes2_smp_timers_init(&memory->timers);
    return mipsnes2_smp_memory_reset(memory);
}

bool mipsnes2_smp_memory_reset(mipsnes2_smp_memory *memory)
{
    if (memory == NULL || !memory->initialized || memory->ram == NULL ||
        memory->ports == NULL ||
        !memory->ports->initialized) {
        return false;
    }
    memory->ipl_enabled = true;
    mipsnes2_smp_timers_init(&memory->timers);
    memory->initialized = true;
    return true;
}

bool mipsnes2_smp_memory_read(mipsnes2_smp_memory *memory,
                              uint16_t address, uint8_t *value)
{
    if (memory == NULL || !memory->initialized || value == NULL ||
        memory->ports == NULL || !memory->ports->initialized) {
        return false;
    }
    if (address >= UINT16_C(0x00f0) && address <= UINT16_C(0x00ff)) {
        if (address >= UINT16_C(0x00f4) && address <= UINT16_C(0x00f7)) {
            return mipsnes2_apu_io_ports_smp_read(
                memory->ports, address - UINT16_C(0x00f4), value);
        }
        if (address >= UINT16_C(0x00fa) &&
            address <= UINT16_C(0x00fc)) {
            *value = 0U;
            return true;
        }
        if (address >= UINT16_C(0x00fd)) {
            return mipsnes2_smp_timers_read_output(
                &memory->timers, address - UINT16_C(0x00fd), value);
        }
        if (address != UINT16_C(0x00f8) &&
            address != UINT16_C(0x00f9)) {
            return false;
        }
    }
    if (address >= UINT16_C(0xffc0) && memory->ipl_enabled) {
        if (memory->ipl == NULL) {
            return false;
        }
        *value = memory->ipl[address - UINT16_C(0xffc0)];
        return true;
    }
    *value = memory->ram[address];
    return true;
}

bool mipsnes2_smp_memory_write(mipsnes2_smp_memory *memory,
                               uint16_t address, uint8_t value)
{
    if (memory == NULL || !memory->initialized || memory->ports == NULL ||
        !memory->ports->initialized) {
        return false;
    }
    if (address >= UINT16_C(0x00f0) && address <= UINT16_C(0x00ff)) {
        if (address == UINT16_C(0x00f1)) {
            if (!mipsnes2_apu_io_ports_smp_clear_inputs(
                    memory->ports, (value & UINT8_C(0x10)) != 0U,
                    (value & UINT8_C(0x20)) != 0U) ||
                !mipsnes2_smp_timers_set_enabled(
                    &memory->timers, value & UINT8_C(0x07))) {
                return false;
            }
            memory->ipl_enabled = (value & UINT8_C(0x80)) != 0U;
        } else if (address >= UINT16_C(0x00f4) &&
                   address <= UINT16_C(0x00f7)) {
            if (!mipsnes2_apu_io_ports_smp_write(
                    memory->ports, address - UINT16_C(0x00f4), value)) {
                return false;
            }
        } else if (address >= UINT16_C(0x00fa) &&
                   address <= UINT16_C(0x00fc)) {
            if (!mipsnes2_smp_timers_write_target(
                    &memory->timers, address - UINT16_C(0x00fa), value)) {
                return false;
            }
        } else if (address >= UINT16_C(0x00fd)) {
            /* Hardware ignores writes to the output counters. */
        } else if (address != UINT16_C(0x00f8) &&
                   address != UINT16_C(0x00f9)) {
            return false;
        }
    }
    memory->ram[address] = value;
    return true;
}

bool mipsnes2_smp_memory_advance(mipsnes2_smp_memory *memory,
                                 uint64_t smp_cycles)
{
    if (memory == NULL || !memory->initialized) {
        return false;
    }
    return mipsnes2_smp_timers_advance(&memory->timers, smp_cycles);
}
