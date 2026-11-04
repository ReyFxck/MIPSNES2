/* Host checks for bounded PS2 mounted-filesystem ROM discovery. GPL-3.0. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "platform/ps2/rom_discovery.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static int write_byte(const char *path, unsigned char value)
{
    FILE *file = fopen(path, "wb");
    int write_result;
    int close_result;

    if (file == NULL) {
        return 1;
    }
    write_result = fputc((int)value, file);
    close_result = fclose(file);
    if (write_result == EOF || close_result != 0) {
        return 1;
    }
    return 0;
}

static int join(char *out, size_t capacity, const char *root, const char *name)
{
    int count = snprintf(out, capacity, "%s/%s", root, name);

    return count < 0 || (size_t)count >= capacity;
}

int main(void)
{
    char base[] = "/tmp/mipsnes2-ps2-rom-discovery-XXXXXX";
    char primary[256];
    char secondary[256];
    char missing[256];
    char path[512];
    char expected[512];
    char tiny[8];
    char file_path[512];
    char executable_path[512];
    char sidecar_sfc[512];
    char sidecar_smc[512];
    const char *roots[3];
    mipsnes2_ps2_rom_discovery_report report;
    mipsnes2_ps2_rom_discovery_result result;

    CHECK(mkdtemp(base) != NULL);
    CHECK(!join(primary, sizeof(primary), base, "primary"));
    CHECK(!join(secondary, sizeof(secondary), base, "secondary"));
    CHECK(!join(missing, sizeof(missing), base, "missing"));
    CHECK(mkdir(primary, 0700) == 0);
    CHECK(mkdir(secondary, 0700) == 0);

    CHECK(!join(executable_path, sizeof(executable_path), base,
                "mipsnes2.elf"));
    CHECK(!join(sidecar_sfc, sizeof(sidecar_sfc), base, "mipsnes2.sfc"));
    CHECK(!join(sidecar_smc, sizeof(sidecar_smc), base, "mipsnes2.smc"));
    CHECK(write_byte(sidecar_smc, 0x44U) == 0);
    CHECK(mipsnes2_ps2_rom_discover_sidecar(
              executable_path, path, sizeof(path)) ==
          MIPSNES2_PS2_ROM_DISCOVERY_OK);
    CHECK(strcmp(path, sidecar_smc) == 0);
    CHECK(write_byte(sidecar_sfc, 0x33U) == 0);
    CHECK(mipsnes2_ps2_rom_discover_sidecar(
              executable_path, path, sizeof(path)) ==
          MIPSNES2_PS2_ROM_DISCOVERY_OK);
    CHECK(strcmp(path, sidecar_sfc) == 0);
    CHECK(mipsnes2_ps2_rom_discover_sidecar(
              executable_path, tiny, sizeof(tiny)) ==
          MIPSNES2_PS2_ROM_DISCOVERY_PATH_TOO_LONG);
    CHECK(tiny[0] == '\0');

    CHECK(!join(file_path, sizeof(file_path), primary, "zeta.SFC"));
    CHECK(write_byte(file_path, 0x5aU) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "Alpha.smc"));
    CHECK(write_byte(file_path, 0xa5U) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "ignore.txt"));
    CHECK(write_byte(file_path, 0x11U) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "empty.sfc"));
    CHECK(write_byte(file_path, 0x00U) == 0);
    CHECK(truncate(file_path, 0) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "folder.sfc"));
    CHECK(mkdir(file_path, 0700) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "unreadable.sfc"));
    CHECK(symlink("missing-target", file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), secondary, "000.sfc"));
    CHECK(write_byte(file_path, 0x22U) == 0);

    roots[0] = missing;
    roots[1] = primary;
    roots[2] = secondary;
    result = mipsnes2_ps2_rom_discover(
        roots, 3U, MIPSNES2_PS2_ROM_DISCOVERY_MAX_ENTRIES_PER_ROOT,
        path, sizeof(path), &report);
    CHECK(result == MIPSNES2_PS2_ROM_DISCOVERY_OK);
    CHECK(!join(expected, sizeof(expected), primary, "Alpha.smc"));
    CHECK(strcmp(path, expected) == 0);
    CHECK(report.roots_attempted == 2U && report.roots_opened == 1U);
    CHECK(report.readable_matches == 2U);
    CHECK(report.extension_matches == 5U);
    CHECK(report.overlong_matches == 0U);

    result = mipsnes2_ps2_rom_discover(
        &roots[1], 1U, MIPSNES2_PS2_ROM_DISCOVERY_MAX_ENTRIES_PER_ROOT,
        tiny, sizeof(tiny), &report);
    CHECK(result == MIPSNES2_PS2_ROM_DISCOVERY_PATH_TOO_LONG);
    CHECK(tiny[0] == '\0' && report.overlong_matches == 5U);

    CHECK(mipsnes2_ps2_rom_discover(
              NULL, 1U, 1U, path, sizeof(path), &report) ==
          MIPSNES2_PS2_ROM_DISCOVERY_INVALID_ARGUMENT);
    CHECK(mipsnes2_ps2_rom_discover(
              roots, 3U, 0U, path, sizeof(path), &report) ==
          MIPSNES2_PS2_ROM_DISCOVERY_INVALID_ARGUMENT);
    CHECK(mipsnes2_ps2_rom_discover(
              roots, MIPSNES2_PS2_ROM_DISCOVERY_MAX_ROOTS + 1U,
              1U, path, sizeof(path), &report) ==
          MIPSNES2_PS2_ROM_DISCOVERY_INVALID_ARGUMENT);

    CHECK(!join(file_path, sizeof(file_path), primary, "zeta.SFC"));
    CHECK(unlink(file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "Alpha.smc"));
    CHECK(unlink(file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "ignore.txt"));
    CHECK(unlink(file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "empty.sfc"));
    CHECK(unlink(file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "folder.sfc"));
    CHECK(rmdir(file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), primary, "unreadable.sfc"));
    CHECK(unlink(file_path) == 0);
    CHECK(!join(file_path, sizeof(file_path), secondary, "000.sfc"));
    CHECK(unlink(file_path) == 0);
    CHECK(unlink(sidecar_sfc) == 0);
    CHECK(unlink(sidecar_smc) == 0);
    CHECK(rmdir(primary) == 0);
    CHECK(rmdir(secondary) == 0);
    CHECK(rmdir(base) == 0);
    (void)puts("PS2 ROM discovery: OK");
    return 0;
}
