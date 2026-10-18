/* MIPSNES2 platform-neutral cartridge preparation. See LICENSE for GPL-3.0 terms. */
#include "core/cartridge/prepare.h"

#include <string.h>

static const mipsnes2_cartridge_header_candidate *selected_candidate(
    const mipsnes2_cartridge_plan *plan)
{
    if (plan->selection.selected_mapper == MIPSNES2_CARTRIDGE_LOROM) {
        return &plan->selection.lorom;
    }
    if (plan->selection.selected_mapper == MIPSNES2_CARTRIDGE_HIROM) {
        return &plan->selection.hirom;
    }
    if (plan->selection.selected_mapper == MIPSNES2_CARTRIDGE_EXHIROM) {
        return &plan->selection.exhirom;
    }
    return NULL;
}

mipsnes2_cartridge_image_result mipsnes2_cartridge_plan_create(
    mipsnes2_cartridge_plan *plan,
    const uint8_t *source,
    size_t source_size,
    mipsnes2_copier_header_policy policy)
{
    mipsnes2_cartridge_plan prepared;
    const mipsnes2_cartridge_header_candidate *candidate;
    mipsnes2_cartridge_image_result result;

    if (plan == NULL || source == NULL) {
        return MIPSNES2_CARTRIDGE_IMAGE_INVALID_ARGUMENT;
    }
    (void)memset(&prepared, 0, sizeof(prepared));
    result = mipsnes2_cartridge_image_view_init(
        &prepared.image, source, source_size, policy);
    if (result != MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return result;
    }
    result = mipsnes2_cartridge_select_mapper(
        &prepared.image, &prepared.selection);
    if (result != MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return result;
    }
    candidate = selected_candidate(&prepared);
    if (candidate == NULL || !candidate->confident ||
        !candidate->header.declared_sram_size_representable) {
        return MIPSNES2_CARTRIDGE_IMAGE_NO_CONFIDENT_HEADER;
    }
    prepared.required_sram_size = candidate->header.declared_sram_size;
    *plan = prepared;
    return MIPSNES2_CARTRIDGE_IMAGE_OK;
}

mipsnes2_cartridge_result mipsnes2_cartridge_construct(
    mipsnes2_cartridge *cartridge,
    const mipsnes2_cartridge_plan *plan,
    uint8_t *sram,
    size_t sram_capacity)
{
    mipsnes2_cartridge_config config;
    mipsnes2_cartridge built;
    const mipsnes2_cartridge_header_candidate *candidate;
    mipsnes2_cartridge_result result;

    if (cartridge == NULL || plan == NULL || plan->image.data == NULL ||
        (sram_capacity != 0U && sram == NULL)) {
        return MIPSNES2_CARTRIDGE_INVALID_ARGUMENT;
    }
    candidate = selected_candidate(plan);
    if (candidate == NULL || !candidate->confident ||
        !candidate->header.declared_sram_size_representable ||
        candidate->header.declared_sram_size != plan->required_sram_size) {
        return MIPSNES2_CARTRIDGE_INVALID_ARGUMENT;
    }
    if (plan->required_sram_size > sram_capacity ||
        (plan->required_sram_size != 0U && sram == NULL)) {
        return MIPSNES2_CARTRIDGE_UNSUPPORTED_SRAM_SIZE;
    }

    config.rom = plan->image.data;
    config.rom_size = plan->image.size;
    config.sram = plan->required_sram_size != 0U ? sram : NULL;
    config.sram_size = plan->required_sram_size;
    config.mapper = plan->selection.selected_mapper;
    result = mipsnes2_cartridge_init(&built, &config);
    if (result != MIPSNES2_CARTRIDGE_OK) {
        return result;
    }
    *cartridge = built;
    return MIPSNES2_CARTRIDGE_OK;
}
