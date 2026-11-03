/* PS2 IOP buffer-loader preparation. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/iop_module_loader.h"

#include <limits.h>
#include <stddef.h>
#include <loadfile.h>
#include <sbv_patches.h>

int mipsnes2_ps2_iop_module_loader_prepare(void)
{
    return sbv_patch_enable_lmb();
}

void mipsnes2_ps2_iop_module_load(
    const char *name, const void *irx, uint32_t irx_size,
    int prepare_result, mipsnes2_ps2_iop_module_load_report *report)
{
    if (report == NULL) {
        return;
    }
    report->before_id = INT_MIN;
    report->load_id = INT_MIN;
    report->start_result = INT_MIN;
    report->after_id = INT_MIN;
    if (prepare_result != 0 || name == NULL || irx == NULL || irx_size == 0U) {
        return;
    }

    report->before_id = SifSearchModuleByName(name);
    report->load_id = SifExecModuleBuffer(
        (void *)irx, irx_size, 0, NULL, &report->start_result);
    report->after_id = SifSearchModuleByName(name);
}
