/* MIPSNES2 cartridge-image metadata foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CARTRIDGE_IMAGE_H
#define MIPSNES2_CARTRIDGE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/cartridge/cartridge.h"

enum {
    MIPSNES2_COPIER_HEADER_SIZE = 512,
    MIPSNES2_INTERNAL_TITLE_SIZE = 21,
    MIPSNES2_INTERNAL_HEADER_BLOCK_SIZE = 64,
    MIPSNES2_LOROM_HEADER_OFFSET = 0x7fc0,
    MIPSNES2_HIROM_HEADER_OFFSET = 0xffc0
};

typedef enum {
    MIPSNES2_COPIER_HEADER_DETECT = 0,
    MIPSNES2_COPIER_HEADER_REJECT,
    MIPSNES2_COPIER_HEADER_REQUIRE
} mipsnes2_copier_header_policy;

typedef enum {
    MIPSNES2_CARTRIDGE_IMAGE_OK = 0,
    MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT,
    MIPSNES2_CARTRIDGE_IMAGE_INVALID_POLICY,
    MIPSNES2_CARTRIDGE_IMAGE_MALFORMED_SIZE,
    MIPSNES2_CARTRIDGE_IMAGE_COPIER_HEADER_FORBIDDEN,
    MIPSNES2_CARTRIDGE_IMAGE_COPIER_HEADER_REQUIRED,
    MIPSNES2_CARTRIDGE_IMAGE_HEADER_OUT_OF_RANGE
} mipsnes2_cartridge_image_result;

typedef enum {
    MIPSNES2_HEADER_MAP_OTHER = 0,
    MIPSNES2_HEADER_MAP_BASE_LOROM,
    MIPSNES2_HEADER_MAP_BASE_HIROM
} mipsnes2_cartridge_header_map;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t source_size;
    size_t source_offset;
    bool had_copier_header;
} mipsnes2_cartridge_image_view;

typedef struct {
    uint8_t title[MIPSNES2_INTERNAL_TITLE_SIZE];
    uint8_t map_mode;
    uint8_t chipset;
    uint8_t rom_size_exponent;
    uint8_t sram_size_exponent;
    uint8_t destination_code;
    uint8_t developer_id;
    uint8_t version;
    uint16_t checksum_complement;
    uint16_t checksum;
    uint16_t reset_vector;
    size_t declared_rom_size;
    size_t declared_sram_size;
    mipsnes2_cartridge_header_map map;
    bool fastrom;
    bool checksum_pair_valid;
    bool declared_rom_size_representable;
    bool declared_sram_size_representable;
} mipsnes2_cartridge_header;

mipsnes2_cartridge_image_result mipsnes2_cartridge_image_view_init(
    mipsnes2_cartridge_image_view *view,
    const uint8_t *source,
    size_t source_size,
    mipsnes2_copier_header_policy policy);
mipsnes2_cartridge_image_result mipsnes2_cartridge_header_parse(
    const mipsnes2_cartridge_image_view *view,
    mipsnes2_cartridge_mapper candidate_mapper,
    mipsnes2_cartridge_header *header);

#endif
