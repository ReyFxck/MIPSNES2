/* MIPSNES2 platform-neutral cartridge preparation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CARTRIDGE_PREPARE_H
#define MIPSNES2_CARTRIDGE_PREPARE_H

#include <stddef.h>
#include <stdint.h>

#include "core/cartridge/cartridge.h"
#include "core/cartridge/image.h"

typedef struct {
    mipsnes2_cartridge_image_view image;
    mipsnes2_cartridge_header_selection selection;
    size_t required_sram_size;
} mipsnes2_cartridge_plan;

/*
 * The source buffer remains caller-owned and must outlive the plan and cartridge.
 * On failure, plan is unchanged.
 */
mipsnes2_cartridge_image_result mipsnes2_cartridge_plan_create(
    mipsnes2_cartridge_plan *plan,
    const uint8_t *source,
    size_t source_size,
    mipsnes2_copier_header_policy policy);

/*
 * SRAM is caller-owned. Capacity may exceed the header-declared requirement, but
 * only the declared byte count becomes visible to the cartridge. On failure,
 * cartridge is unchanged.
 */
mipsnes2_cartridge_result mipsnes2_cartridge_construct(
    mipsnes2_cartridge *cartridge,
    const mipsnes2_cartridge_plan *plan,
    uint8_t *sram,
    size_t sram_capacity);

#endif
