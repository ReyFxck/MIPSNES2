/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#include <stdio.h>
#include "core/common/log.h"
#include "core/common/project.h"
static void host_log(void *context, mipsnes2_log_level level, const char *message)
{
    FILE *stream = context;
    (void)level;
    (void)fprintf(stream, "%s\n", message);
}
int main(int argc, char **argv)
{
    mipsnes2_logger logger = {host_log, stdout};
    (void)argv;
    if (argc != 1) {
        (void)fputs("Foundation probe accepts no arguments; ROM execution is unavailable.\n", stderr);
        return 2;
    }
    mipsnes2_log(&logger, MIPSNES2_LOG_INFO, MIPSNES2_BANNER);
    return ferror(stdout) ? 1 : 0;
}
