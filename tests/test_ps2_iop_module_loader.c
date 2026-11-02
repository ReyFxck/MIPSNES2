/* PS2 IOP loader contract fixture. See LICENSE for GPL-3.0 terms. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "platform/ps2/iop_module_loader.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; \
} } while (0)

enum { PATCH = 1, SEARCH = 2, LOAD = 3 };
static int events[4];
static unsigned int event_count;
static int patch_result;
static int load_result;
static int start_result;
static int searches;
static const unsigned char image[4] = {1U, 2U, 3U, 4U};

int sbv_patch_enable_lmb(void)
{
    events[event_count++] = PATCH;
    return patch_result;
}

int SifSearchModuleByName(const char *name)
{
    if (strcmp(name, "rfauds2") != 0 || event_count == 0U ||
        events[0] != PATCH) {
        return -99;
    }
    events[event_count++] = SEARCH;
    return ++searches == 1 ? 0 : 17;
}

int SifExecModuleBuffer(void *ptr, uint32_t size, uint32_t arg_len,
                        const char *args, int *mod_res)
{
    if (event_count != 2U || events[0] != PATCH || events[1] != SEARCH ||
        ptr != image || size != sizeof(image) || arg_len != 0U ||
        args != NULL || mod_res == NULL) {
        return -99;
    }
    events[event_count++] = LOAD;
    *mod_res = start_result;
    return load_result;
}

int main(void)
{
    mipsnes2_ps2_iop_module_load_report report;
    int prepared;

    patch_result = 0;
    load_result = 17;
    start_result = 0;
    prepared = mipsnes2_ps2_iop_module_loader_prepare();
    CHECK(prepared == 0);
    mipsnes2_ps2_iop_module_load(
        "rfauds2", image, sizeof(image), prepared, &report);
    CHECK(report.before_id == 0 && report.load_id == 17 &&
          report.start_result == 0 && report.after_id == 17);
    CHECK(event_count == 4U && events[0] == PATCH && events[1] == SEARCH &&
          events[2] == LOAD && events[3] == SEARCH);

    event_count = 0U;
    searches = 0;
    patch_result = 1;
    prepared = mipsnes2_ps2_iop_module_loader_prepare();
    CHECK(prepared == 1);
    mipsnes2_ps2_iop_module_load(
        "rfauds2", image, sizeof(image), prepared, &report);
    CHECK(event_count == 1U && events[0] == PATCH);
    CHECK(report.before_id == INT_MIN && report.load_id == INT_MIN &&
          report.start_result == INT_MIN && report.after_id == INT_MIN);

    event_count = 0U;
    searches = 0;
    patch_result = 0;
    load_result = -12;
    prepared = mipsnes2_ps2_iop_module_loader_prepare();
    mipsnes2_ps2_iop_module_load(
        "rfauds2", image, sizeof(image), prepared, &report);
    CHECK(report.load_id == -12 && report.after_id == 17 &&
          event_count == 4U);

    (void)puts("PS2 IOP buffer-loader sequence: OK");
    return 0;
}
