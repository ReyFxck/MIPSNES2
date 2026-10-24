/* MIPSNES2 SNES logical bus foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_SNES_BUS_H
#define MIPSNES2_SNES_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/bus/cpu_io.h"
#include "core/bus/interface.h"
#include "core/cartridge/interface.h"
#include "core/dma/registers.h"
#include "core/ppu/counters.h"

enum {
    MIPSNES2_SNES_WRAM_SIZE = 128 * 1024
};

typedef enum {
    MIPSNES2_SNES_BUS_OK = 0,
    MIPSNES2_SNES_BUS_INVALID_ARGUMENT,
    MIPSNES2_SNES_BUS_TIMING_OVERFLOW
} mipsnes2_snes_bus_result;

typedef struct {
    uint8_t *wram;
    size_t wram_size;
    uint8_t initial_open_value;
    mipsnes2_bus_interface ppu;
    mipsnes2_bus_interface apu;
    mipsnes2_bus_interface cpu_io;
    mipsnes2_cpu_io *cpu_io_state;
    mipsnes2_ppu_counters *ppu_counters;
    const mipsnes2_hv_timing *hv_timing;
    mipsnes2_bus_interface dma;
    mipsnes2_dma_registers *dma_registers;
    mipsnes2_cartridge_interface cartridge;
} mipsnes2_snes_bus_config;

typedef struct {
    uint8_t *wram;
    mipsnes2_bus_interface ppu;
    mipsnes2_bus_interface apu;
    mipsnes2_bus_interface cpu_io;
    mipsnes2_cpu_io *cpu_io_state;
    mipsnes2_ppu_counters *ppu_counters;
    const mipsnes2_hv_timing *hv_timing;
    mipsnes2_bus_interface dma;
    mipsnes2_dma_registers *dma_registers;
    mipsnes2_cartridge_interface cartridge;
    uint32_t wram_port_address;
    uint64_t access_count;
    uint64_t master_clocks;
    uint8_t open_value;
    uint8_t dma_request_mask;
    uint8_t dma_grace_cycles;
    bool fastrom_enabled;
    bool timing_overflow;
} mipsnes2_snes_bus;

mipsnes2_snes_bus_result mipsnes2_snes_bus_init(
    mipsnes2_snes_bus *bus,
    const mipsnes2_snes_bus_config *config);
uint8_t mipsnes2_snes_bus_read(mipsnes2_snes_bus *bus,
                               mipsnes2_address address);
void mipsnes2_snes_bus_write(mipsnes2_snes_bus *bus,
                             mipsnes2_address address,
                             uint8_t value);
/* Account a qualified bus write whose external target did not accept it.
 * This is a caller-selected bus policy, not automatic 5A22 ABORT behavior. */
mipsnes2_snes_bus_result mipsnes2_snes_bus_account_inhibited_write(
    mipsnes2_snes_bus *bus, mipsnes2_address address, uint8_t value);
mipsnes2_snes_bus_result mipsnes2_snes_bus_account_internal_cycles(
    mipsnes2_snes_bus *bus,
    uint64_t processor_cycles);
mipsnes2_bus_interface mipsnes2_snes_bus_make_interface(
    mipsnes2_snes_bus *bus);
bool mipsnes2_snes_bus_interface_matches(
    const mipsnes2_snes_bus *bus,
    mipsnes2_bus_interface interface);
/* Call immediately after external I/O changes, before advancing emulated time. */
bool mipsnes2_snes_bus_sync_hv_latch(mipsnes2_snes_bus *bus);
/* One untimed DMA data phase. No CPU cycle/access counters or math cycles
 * are charged. The timing owner must charge one eight-master-clock slot. */
void mipsnes2_snes_bus_dma_byte(mipsnes2_snes_bus *bus,
    mipsnes2_address a_address, mipsnes2_address b_address, bool b_to_a);

#endif
