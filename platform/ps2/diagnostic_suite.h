/* MIPSNES2 PS2 diagnostic contract. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_DIAGNOSTIC_SUITE_H
#define MIPSNES2_PS2_DIAGNOSTIC_SUITE_H

#include <stdbool.h>

typedef void (*mipsnes2_ps2_diagnostic_emit)(void *context,
                                              const char *name, bool passed);

typedef struct {
    unsigned passed;
    unsigned failed;
} mipsnes2_ps2_diagnostic_report;

/* Original embedded fixtures using the shared SNES core. The callback is
 * owned by the target presentation; no test ROM or firmware is bundled. */
void mipsnes2_ps2_diagnostic_run(mipsnes2_ps2_diagnostic_report *report,
                                 mipsnes2_ps2_diagnostic_emit emit,
                                 void *context);

#endif
