/* Host check for the PS2 file adapter, with our own temporary bytes. GPL-3.0. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform/ps2/rom_file.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

int main(void)
{
    char path[] = "/tmp/mipsnes2-ps2-rom-file-XXXXXX";
    const uint8_t bytes[] = {0x5aU, 0x00U, 0xffU, 0x21U};
    uint8_t buffer[sizeof(bytes)];
    size_t size = 99U;
    FILE *file;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    file = fdopen(fd, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes));
    CHECK(fclose(file) == 0);
    (void)memset(buffer, 0U, sizeof(buffer));
    CHECK(mipsnes2_ps2_rom_file_load(path, buffer, sizeof(buffer), &size) ==
          MIPSNES2_PS2_ROM_FILE_OK);
    CHECK(size == sizeof(bytes) && memcmp(buffer, bytes, size) == 0);
    CHECK(mipsnes2_ps2_rom_file_load(path, buffer, sizeof(buffer) - 1U,
                                    &size) == MIPSNES2_PS2_ROM_FILE_TOO_LARGE);
    CHECK(size == 0U);
    CHECK(mipsnes2_ps2_rom_file_load(NULL, buffer, sizeof(buffer), &size) ==
          MIPSNES2_PS2_ROM_FILE_INVALID_ARGUMENT && size == 0U);
    file = fopen(path, "wb");
    CHECK(file != NULL && fclose(file) == 0);
    CHECK(mipsnes2_ps2_rom_file_load(path, buffer, sizeof(buffer), &size) ==
          MIPSNES2_PS2_ROM_FILE_EMPTY && size == 0U);
    CHECK(unlink(path) == 0);
    CHECK(mipsnes2_ps2_rom_file_load(path, buffer, sizeof(buffer), &size) ==
          MIPSNES2_PS2_ROM_FILE_OPEN_ERROR && size == 0U);
    (void)puts("PS2 ROM file adapter: OK");
    return 0;
}
