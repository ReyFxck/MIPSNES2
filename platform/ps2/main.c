/* MIPSNES2 PS2 diagnostic launcher. See LICENSE for GPL-3.0 terms. */
#include <stdio.h>
#include <unistd.h>

#include <debug.h>
#include <sifrpc.h>

#include "platform/ps2/diagnostic_suite.h"

static void print_result(void *context, const char *name, bool passed)
{
    (void)context;
    scr_printf("%s  %s\n", passed ? "PASS" : "FAIL", name);
    (void)printf("MIPSNES2 PS2 DIAG: %s %s\n",
                 passed ? "PASS" : "FAIL", name);
}

int main(void)
{
    mipsnes2_ps2_diagnostic_report report;

    sceSifInitRpc(0);
    init_scr();
    scr_printf("MIPSNES2 PS2 DIAGNOSTIC v1\n\n");
    mipsnes2_ps2_diagnostic_run(&report, print_result, NULL);
    scr_printf("\nRESULT: %u PASS / %u FAIL\n", report.passed, report.failed);
    scr_printf("Use Android Back to exit.\n");
    (void)printf("MIPSNES2 PS2 DIAG: RESULT %u PASS / %u FAIL\n",
                 report.passed, report.failed);
    (void)fflush(stdout);
    for (;;) {
        (void)sleep(1);
    }
}
