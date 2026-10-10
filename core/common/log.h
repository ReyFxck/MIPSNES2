/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_LOG_H
#define MIPSNES2_LOG_H
#ifndef MIPSNES2_ENABLE_LOGGING
#define MIPSNES2_ENABLE_LOGGING 1
#endif
/* Synchronous borrowed message; a sink must copy it if retaining it. */
typedef enum {
    MIPSNES2_LOG_INFO,
    MIPSNES2_LOG_WARNING,
    MIPSNES2_LOG_ERROR
} mipsnes2_log_level;
typedef void (*mipsnes2_log_sink)(void *context, mipsnes2_log_level level,
                                const char *message);
typedef struct {
    mipsnes2_log_sink sink;
    void *context;
} mipsnes2_logger;
/* A null logger, null sink or null message is ignored. No allocation or I/O. */
void mipsnes2_log(const mipsnes2_logger *logger, mipsnes2_log_level level,
                  const char *message);
#endif
