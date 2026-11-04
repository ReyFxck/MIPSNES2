/* Bounded PS2 mounted-filesystem ROM discovery. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_ROM_DISCOVERY_H
#define MIPSNES2_PS2_ROM_DISCOVERY_H

#include <stdbool.h>
#include <stddef.h>

enum {
    MIPSNES2_PS2_ROM_DISCOVERY_MAX_ROOTS = 16,
    MIPSNES2_PS2_ROM_DISCOVERY_MAX_ENTRIES_PER_ROOT = 512,
    MIPSNES2_PS2_ROM_DISCOVERY_PATH_CAPACITY = 512
};

typedef enum {
    MIPSNES2_PS2_ROM_DISCOVERY_OK = 0,
    MIPSNES2_PS2_ROM_DISCOVERY_INVALID_ARGUMENT,
    MIPSNES2_PS2_ROM_DISCOVERY_NOT_FOUND,
    MIPSNES2_PS2_ROM_DISCOVERY_PATH_TOO_LONG
} mipsnes2_ps2_rom_discovery_result;

typedef struct {
    size_t roots_attempted;
    size_t roots_opened;
    size_t entries_examined;
    size_t extension_matches;
    size_t readable_matches;
    size_t overlong_matches;
    bool entry_limit_reached;
} mipsnes2_ps2_rom_discovery_report;

/* Tries a nonempty .sfc sidecar and then a .smc sidecar obtained by replacing
 * the executable path's final extension. This also supports host: content-URI
 * paths supplied as argv[0] by development emulators.
 */
mipsnes2_ps2_rom_discovery_result mipsnes2_ps2_rom_discover_sidecar(
    const char *executable_path, char *path_out, size_t path_capacity);

/* Scans each already-mounted root non-recursively and in caller priority order.
 * Within the first root containing a readable, nonempty .sfc/.smc file, the
 * bytewise-lexicographically smallest path is selected. Work is bounded by the
 * root and entry limits above; no storage is allocated by this adapter.
 */
mipsnes2_ps2_rom_discovery_result mipsnes2_ps2_rom_discover(
    const char *const *roots, size_t root_count,
    size_t max_entries_per_root, char *path_out, size_t path_capacity,
    mipsnes2_ps2_rom_discovery_report *report);

#endif
