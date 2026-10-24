/* PS2 file adapter for caller-owned ROM memory. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/rom_file.h"

#include <stdio.h>

mipsnes2_ps2_rom_file_result mipsnes2_ps2_rom_file_load(
    const char *path, uint8_t *buffer, size_t capacity, size_t *size_out)
{
    FILE *file;
    size_t size = 0U;
    mipsnes2_ps2_rom_file_result result = MIPSNES2_PS2_ROM_FILE_OK;

    if (size_out == NULL) {
        return MIPSNES2_PS2_ROM_FILE_INVALID_ARGUMENT;
    }
    *size_out = 0U;
    if (path == NULL || path[0] == '\0' || buffer == NULL || capacity == 0U) {
        return MIPSNES2_PS2_ROM_FILE_INVALID_ARGUMENT;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return MIPSNES2_PS2_ROM_FILE_OPEN_ERROR;
    }
    while (size < capacity) {
        size_t chunk = capacity - size;
        size_t count;

        if (chunk > 8192U) {
            chunk = 8192U;
        }
        count = fread(buffer + size, 1U, chunk, file);
        size += count;
        if (count < chunk) {
            if (ferror(file)) {
                result = MIPSNES2_PS2_ROM_FILE_READ_ERROR;
                break;
            }
            if (feof(file)) {
                break;
            }
            if (count == 0U) {
                result = MIPSNES2_PS2_ROM_FILE_READ_ERROR;
                break;
            }
        }
    }
    if (result == MIPSNES2_PS2_ROM_FILE_OK && size == capacity) {
        int extra = fgetc(file);

        if (extra != EOF) {
            result = MIPSNES2_PS2_ROM_FILE_TOO_LARGE;
        } else if (ferror(file)) {
            result = MIPSNES2_PS2_ROM_FILE_READ_ERROR;
        }
    }
    if (result == MIPSNES2_PS2_ROM_FILE_OK && size == 0U) {
        result = MIPSNES2_PS2_ROM_FILE_EMPTY;
    }
    if (fclose(file) != 0 && result == MIPSNES2_PS2_ROM_FILE_OK) {
        result = MIPSNES2_PS2_ROM_FILE_READ_ERROR;
    }
    if (result == MIPSNES2_PS2_ROM_FILE_OK) {
        *size_out = size;
    }
    return result;
}
