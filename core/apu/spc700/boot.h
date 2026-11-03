/* MIPSNES2 clean-room S-SMP boot protocol. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_SMP_BOOT_H
#define MIPSNES2_SMP_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#include "core/apu/spc700/cpu.h"

typedef enum {
    MIPSNES2_SMP_BOOT_WAIT_COMMAND = 0,
    MIPSNES2_SMP_BOOT_WAIT_FIRST_BYTE,
    MIPSNES2_SMP_BOOT_TRANSFER,
    MIPSNES2_SMP_BOOT_COMPLETE,
    MIPSNES2_SMP_BOOT_FAILED
} mipsnes2_smp_boot_state;

typedef struct {
    bool start_cpu;
    mipsnes2_spc700_registers registers;
} mipsnes2_smp_boot_event;

/* Behavioral replacement for the documented power-on upload protocol. It
 * contains no Nintendo IPL bytes and does not expose a firmware image. */
typedef struct {
    mipsnes2_smp_memory *memory;
    uint16_t destination;
    uint8_t expected_index;
    mipsnes2_smp_boot_state state;
    bool initialized;
} mipsnes2_smp_boot;

bool mipsnes2_smp_boot_init(mipsnes2_smp_boot *boot,
                            mipsnes2_smp_memory *memory);
bool mipsnes2_smp_boot_reset(mipsnes2_smp_boot *boot);

/* Records one S-CPU write to $2140-$217F and advances the upload protocol.
 * On success event is always initialized; start_cpu is true exactly once. */
bool mipsnes2_smp_boot_cpu_write(mipsnes2_smp_boot *boot,
                                 uint16_t address, uint8_t value,
                                 mipsnes2_smp_boot_event *event);

#endif
