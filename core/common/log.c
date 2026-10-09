/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#include "log.h"
#include <stddef.h>
void mipsnes2_log(const mipsnes2_logger *logger, mipsnes2_log_level level,
                  const char *message)
{
#if MIPSNES2_ENABLE_LOGGING
    if (logger != NULL && logger->sink != NULL && message != NULL) {
        logger->sink(logger->context, level, message);
    }
#else
    (void)logger;
    (void)level;
    (void)message;
#endif
}
