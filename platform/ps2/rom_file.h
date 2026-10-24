/* PS2 file adapter for caller-owned ROM memory. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_ROM_FILE_H
#define MIPSNES2_PS2_ROM_FILE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MIPSNES2_PS2_ROM_FILE_OK = 0,
    MIPSNES2_PS2_ROM_FILE_INVALID_ARGUMENT,
    MIPSNES2_PS2_ROM_FILE_OPEN_ERROR,
    MIPSNES2_PS2_ROM_FILE_READ_ERROR,
    MIPSNES2_PS2_ROM_FILE_EMPTY,
    MIPSNES2_PS2_ROM_FILE_TOO_LARGE
} mipsnes2_ps2_rom_file_result;

/* Reads a path already accessible through the target's stdio filesystem.
 * No device drivers are mounted here. On failure size_out is zero and the
 * buffer's partial contents must not be used.
 */
mipsnes2_ps2_rom_file_result mipsnes2_ps2_rom_file_load(
    const char *path, uint8_t *buffer, size_t capacity, size_t *size_out);

#endif
