/* PS2 IOP buffer-loader preparation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_IOP_MODULE_LOADER_H
#define MIPSNES2_PS2_IOP_MODULE_LOADER_H

#include <stdint.h>

typedef struct {
    int before_id;
    int load_id;
    int start_result;
    int after_id;
} mipsnes2_ps2_iop_module_load_report;

/* Call once after SIF RPC initialization, before loading any EE-embedded IRX.
 * Zero means the ROM LOADFILE service is ready for LoadModuleBuffer RPC. */
int mipsnes2_ps2_iop_module_loader_prepare(void);

/* A failed preparation never attempts the buffer load. INT_MIN identifies
 * fields that were not observed, not an IOP return code. */
void mipsnes2_ps2_iop_module_load(
    const char *name, const void *irx, uint32_t irx_size,
    int prepare_result, mipsnes2_ps2_iop_module_load_report *report);

#endif
