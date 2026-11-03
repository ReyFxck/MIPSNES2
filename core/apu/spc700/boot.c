/* MIPSNES2 clean-room S-SMP boot protocol. See LICENSE for GPL-3.0 terms. */
#include "core/apu/spc700/boot.h"

#include <string.h>

static bool read_input(const mipsnes2_smp_boot *boot, unsigned port,
                       uint8_t *value)
{
    return mipsnes2_apu_io_ports_smp_read(
        boot->memory->ports, port, value);
}

static bool acknowledge(const mipsnes2_smp_boot *boot, uint8_t value)
{
    return mipsnes2_apu_io_ports_smp_write(
        boot->memory->ports, 0U, value);
}

static bool read_destination(const mipsnes2_smp_boot *boot,
                             uint16_t *destination)
{
    uint8_t low;
    uint8_t high;

    if (destination == NULL || !read_input(boot, 2U, &low) ||
        !read_input(boot, 3U, &high)) {
        return false;
    }
    *destination = (uint16_t)(low | ((uint16_t)high << 8));
    return true;
}

static bool begin_command(mipsnes2_smp_boot *boot, uint8_t signal,
                          mipsnes2_smp_boot_event *event)
{
    uint8_t command;

    if (!read_input(boot, 1U, &command) ||
        !read_destination(boot, &boot->destination) ||
        !acknowledge(boot, signal)) {
        return false;
    }
    if (command != 0U) {
        boot->expected_index = 0U;
        boot->state = MIPSNES2_SMP_BOOT_WAIT_FIRST_BYTE;
        return true;
    }

    event->start_cpu = true;
    event->registers.pc = boot->destination;
    event->registers.sp = UINT8_C(0xef);
    event->registers.psw = MIPSNES2_SPC700_FLAG_Z;
    boot->state = MIPSNES2_SMP_BOOT_COMPLETE;
    return true;
}

static bool receive_byte(mipsnes2_smp_boot *boot, uint8_t signal)
{
    uint8_t byte;

    if (!read_input(boot, 1U, &byte) ||
        !mipsnes2_smp_memory_write(
            boot->memory, boot->destination, byte) ||
        !acknowledge(boot, signal)) {
        return false;
    }
    boot->destination = (uint16_t)(boot->destination + UINT16_C(1));
    boot->expected_index = (uint8_t)(signal + UINT8_C(1));
    boot->state = MIPSNES2_SMP_BOOT_TRANSFER;
    return true;
}

bool mipsnes2_smp_boot_init(mipsnes2_smp_boot *boot,
                            mipsnes2_smp_memory *memory)
{
    if (boot == NULL || memory == NULL || !memory->initialized ||
        memory->ram == NULL || memory->ports == NULL ||
        !memory->ports->initialized) {
        return false;
    }
    *boot = (mipsnes2_smp_boot){0};
    boot->memory = memory;
    boot->initialized = true;
    return mipsnes2_smp_boot_reset(boot);
}

bool mipsnes2_smp_boot_reset(mipsnes2_smp_boot *boot)
{
    if (boot == NULL || !boot->initialized || boot->memory == NULL ||
        !boot->memory->initialized || boot->memory->ram == NULL ||
        boot->memory->ports == NULL ||
        !boot->memory->ports->initialized) {
        return false;
    }
    (void)memset(boot->memory->ram, 0, UINT16_C(0x00f0));
    boot->destination = 0U;
    boot->expected_index = 0U;
    boot->state = MIPSNES2_SMP_BOOT_WAIT_COMMAND;
    return mipsnes2_apu_io_ports_smp_write(
               boot->memory->ports, 0U, UINT8_C(0xaa)) &&
           mipsnes2_apu_io_ports_smp_write(
               boot->memory->ports, 1U, UINT8_C(0xbb));
}

bool mipsnes2_smp_boot_cpu_write(mipsnes2_smp_boot *boot,
                                 uint16_t address, uint8_t value,
                                 mipsnes2_smp_boot_event *event)
{
    unsigned port;
    uint8_t delta;
    bool ok = true;

    if (boot == NULL || !boot->initialized || event == NULL ||
        boot->memory == NULL || boot->memory->ports == NULL ||
        address < UINT16_C(0x2140) || address > UINT16_C(0x217f)) {
        return false;
    }
    *event = (mipsnes2_smp_boot_event){0};
    if (boot->state == MIPSNES2_SMP_BOOT_FAILED ||
        !mipsnes2_apu_io_ports_cpu_write(
            boot->memory->ports, address, value)) {
        return false;
    }
    port = address & 3U;
    if (port != 0U || boot->state == MIPSNES2_SMP_BOOT_COMPLETE) {
        return true;
    }

    switch (boot->state) {
    case MIPSNES2_SMP_BOOT_WAIT_COMMAND:
        if (value == UINT8_C(0xcc)) {
            ok = begin_command(boot, value, event);
        }
        break;
    case MIPSNES2_SMP_BOOT_WAIT_FIRST_BYTE:
        if (value == 0U) {
            ok = receive_byte(boot, value);
        }
        break;
    case MIPSNES2_SMP_BOOT_TRANSFER:
        if (value == boot->expected_index) {
            ok = receive_byte(boot, value);
            break;
        }
        delta = (uint8_t)(value - boot->expected_index);
        if (delta != 0U && delta < UINT8_C(0x80)) {
            ok = begin_command(boot, value, event);
        }
        break;
    case MIPSNES2_SMP_BOOT_COMPLETE:
        break;
    case MIPSNES2_SMP_BOOT_FAILED:
    default:
        ok = false;
        break;
    }
    if (!ok) {
        boot->state = MIPSNES2_SMP_BOOT_FAILED;
    }
    return ok;
}
