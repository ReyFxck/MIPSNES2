/* MIPSNES2 cartridge mapping foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CARTRIDGE_H
#define MIPSNES2_CARTRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/cartridge/interface.h"

enum {
    MIPSNES2_LOROM_BANK_SIZE = 32 * 1024,
    MIPSNES2_HIROM_BANK_SIZE = 64 * 1024,
    MIPSNES2_BASE_ROM_MAX_SIZE = 4 * 1024 * 1024,
    MIPSNES2_EXHIROM_MAX_SIZE = 8 * 1024 * 1024,
    MIPSNES2_LOROM_SRAM_MAX_SIZE = 512 * 1024,
    MIPSNES2_HIROM_SRAM_MAX_SIZE = 256 * 1024
};

typedef enum {
    MIPSNES2_CARTRIDGE_LOROM = 0,
    MIPSNES2_CARTRIDGE_HIROM,
    MIPSNES2_CARTRIDGE_EXHIROM
} mipsnes2_cartridge_mapper;

typedef enum {
    MIPSNES2_CARTRIDGE_OK = 0,
    MIPSNES2_CARTRIDGE_INVALID_ARGUMENT,
    MIPSNES2_CARTRIDGE_UNSUPPORTED_MAPPER,
    MIPSNES2_CARTRIDGE_UNSUPPORTED_ROM_SIZE,
    MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE
} mipsnes2_cartridge_result;

typedef struct {
    const uint8_t *rom;
    size_t rom_size;
    uint8_t *sram;
    size_t sram_size;
    mipsnes2_cartridge_mapper mapper;
} mipsnes2_cartridge_config;

typedef struct {
    const uint8_t *rom;
    size_t rom_size;
    size_t rom_first_size;
    size_t rom_mirror_span;
    uint8_t *sram;
    size_t sram_size;
    uint64_t sram_revision;
    bool sram_dirty;
    mipsnes2_cartridge_mapper mapper;
} mipsnes2_cartridge;

typedef struct {
    const uint8_t *data;
    size_t size;
    uint64_t revision;
    bool dirty;
} mipsnes2_cartridge_sram_view;

mipsnes2_cartridge_result mipsnes2_cartridge_init(
    mipsnes2_cartridge *cartridge,
    const mipsnes2_cartridge_config *config);
bool mipsnes2_cartridge_read(mipsnes2_cartridge *cartridge,
                             mipsnes2_address address,
                             uint8_t *value);
bool mipsnes2_cartridge_write(mipsnes2_cartridge *cartridge,
                              mipsnes2_address address,
                              uint8_t value);
mipsnes2_cartridge_interface mipsnes2_cartridge_make_interface(
    mipsnes2_cartridge *cartridge);
bool mipsnes2_cartridge_sram_view_get(
    const mipsnes2_cartridge *cartridge,
    mipsnes2_cartridge_sram_view *view);
bool mipsnes2_cartridge_sram_acknowledge(
    mipsnes2_cartridge *cartridge,
    uint64_t revision);

#endif
