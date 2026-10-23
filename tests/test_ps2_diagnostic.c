/* PS2 diagnostic fixture's host baseline. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "platform/ps2/diagnostic_suite.h"

typedef struct {
    unsigned calls;
    unsigned failures;
    const char *first;
} observer;

static void observe(void *context, const char *name, bool passed)
{
    observer *seen = context;

    if (seen->calls == 0U) {
        seen->first = name;
    }
    seen->calls++;
    if (!passed) {
        seen->failures++;
        (void)fprintf(stderr, "FAIL: %s\n", name);
    }
}

int main(void)
{
    mipsnes2_ps2_diagnostic_report report;
    observer seen = {0U, 0U, NULL};

    mipsnes2_ps2_diagnostic_run(&report, observe, &seen);
    if (report.passed != 5U || report.failed != 0U ||
        seen.calls != 5U || seen.failures != 0U || seen.first == NULL ||
        strcmp(seen.first, "CPU LDA ADC STA") != 0) {
        (void)fprintf(stderr, "PS2 fixture: %u passed, %u failed, %u emitted\n",
                      report.passed, report.failed, seen.calls);
        return 1;
    }
    (void)puts("PASS: PS2 diagnostic fixture host baseline (5/5)");
    return 0;
}
