/* MIPSNES2 build probe, not an emulator. See LICENSE for GPL-3.0. */
#include <stdio.h>
#include "core/common/log.h"
#include "core/common/project.h"
static void ps2_probe_log(void *context, mipsnes2_log_level level, const char *message)
{
    (void)context;
    (void)level;
    (void)printf("%s\n", message);
}
int main(void)
{
    const mipsnes2_logger logger = {ps2_probe_log, NULL};
    mipsnes2_log(&logger, MIPSNES2_LOG_INFO, MIPSNES2_BANNER);
    /* This probe deliberately returns. No GS, input or audio backend exists. */
    return 0;
}
