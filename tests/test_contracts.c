/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/common/address.h"
#include "core/common/log.h"
#include "core/bus/interface.h"
#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)
typedef struct {
    unsigned calls;
    mipsnes2_log_level level;
    const char *message;
} capture;
static void capture_log(void *context, mipsnes2_log_level level, const char *message)
{
    capture *output = context;
    output->calls++;
    output->level = level;
    output->message = message;
}
int main(void)
{
    uint32_t bank, offset;
    capture output = {0, MIPSNES2_LOG_INFO, NULL};
    mipsnes2_logger logger = {capture_log, &output};
    mipsnes2_logger empty = {NULL, &output};
    CHECK(mipsnes2_address24(UINT32_MAX) == UINT32_C(0xffffff));
    CHECK(mipsnes2_address24(UINT32_C(0x1000000)) == 0);
    CHECK(mipsnes2_address24(UINT32_C(0xab123456)) == UINT32_C(0x123456));
    for (bank = 0; bank < 256; ++bank) {
        for (offset = 0; offset < 65536; ++offset) {
            mipsnes2_address address = mipsnes2_address_make((uint8_t)bank, (uint16_t)offset);
            CHECK(address == bank * UINT32_C(65536) + offset);
            CHECK(mipsnes2_address24(address | UINT32_C(0xff000000)) == address);
        }
    }
    mipsnes2_log(NULL, MIPSNES2_LOG_ERROR, "ignored");
    mipsnes2_log(&empty, MIPSNES2_LOG_ERROR, "ignored");
    mipsnes2_log(&logger, MIPSNES2_LOG_ERROR, NULL);
    CHECK(output.calls == 0);
    mipsnes2_log(&logger, MIPSNES2_LOG_WARNING, "test-message");
#if MIPSNES2_ENABLE_LOGGING
    CHECK(output.calls == 1);
    CHECK(output.level == MIPSNES2_LOG_WARNING);
    CHECK(strcmp(output.message, "test-message") == 0);
#else
    CHECK(output.calls == 0);
#endif
    (void)puts("PASS: logical address and logging contracts (not SNES emulation)");
    return 0;
}
