/* MIPSNES2 build probe, not an emulator. See LICENSE for GPL-3.0. */
#include <pspuser.h>
#include <stdio.h>
#include "core/common/log.h"
#include "core/common/project.h"
PSP_MODULE_INFO("MIPSNES2 Foundation", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
static void psp_probe_log(void *context, mipsnes2_log_level level, const char *message)
{
    (void)context;
    (void)level;
    (void)printf("%s\n", message);
}
int main(void)
{
    const mipsnes2_logger logger = {psp_probe_log, NULL};
    mipsnes2_log(&logger, MIPSNES2_LOG_INFO, MIPSNES2_BANNER);
    /* No GU, controller or audio backend. Runtime validation is still required. */
    sceKernelExitGame();
    return 0;
}
