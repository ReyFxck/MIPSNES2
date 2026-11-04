/* Bounded PS2 mounted-filesystem ROM discovery. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/rom_discovery.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

static char ascii_lower(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return (char)(value + ('a' - 'A'));
    }
    return value;
}

static bool has_rom_extension(const char *name)
{
    size_t length;

    if (name == NULL) {
        return false;
    }
    length = strlen(name);
    if (length <= 4U || name[length - 4U] != '.') {
        return false;
    }
    return ascii_lower(name[length - 3U]) == 's' &&
        (ascii_lower(name[length - 2U]) == 'f' ||
         ascii_lower(name[length - 2U]) == 'm') &&
        ascii_lower(name[length - 1U]) == 'c';
}

static bool is_plain_name(const char *name)
{
    const char *cursor;

    if (name == NULL || name[0] == '\0') {
        return false;
    }
    for (cursor = name; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\' || *cursor == ':') {
            return false;
        }
    }
    return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static bool join_path(const char *root, const char *name,
                      char *path, size_t capacity)
{
    size_t root_length = strlen(root);
    size_t name_length = strlen(name);
    bool separator = root_length != 0U && root[root_length - 1U] != '/' &&
        root[root_length - 1U] != '\\' && root[root_length - 1U] != ':';
    size_t separator_length = separator ? 1U : 0U;

    if (root_length == 0U || root_length >= capacity ||
        separator_length > capacity - root_length - 1U ||
        name_length > capacity - root_length - separator_length - 1U) {
        return false;
    }
    (void)memcpy(path, root, root_length);
    if (separator) {
        path[root_length] = '/';
    }
    (void)memcpy(path + root_length + separator_length, name,
                 name_length + 1U);
    return true;
}

static bool readable_nonempty_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    int first;

    if (file == NULL) {
        return false;
    }
    first = fgetc(file);
    (void)fclose(file);
    return first != EOF;
}

static bool sidecar_path(const char *executable_path, const char *extension,
                         char *path, size_t capacity)
{
    const char *dot = strrchr(executable_path, '.');
    const char *slash = strrchr(executable_path, '/');
    const char *backslash = strrchr(executable_path, '\\');
    size_t prefix_length;
    size_t extension_length = strlen(extension);

    if (dot == NULL || (slash != NULL && dot < slash) ||
        (backslash != NULL && dot < backslash)) {
        prefix_length = strlen(executable_path);
    } else {
        prefix_length = (size_t)(dot - executable_path);
    }
    if (prefix_length >= capacity ||
        extension_length > capacity - prefix_length - 1U) {
        return false;
    }
    (void)memcpy(path, executable_path, prefix_length);
    (void)memcpy(path + prefix_length, extension, extension_length + 1U);
    return true;
}

mipsnes2_ps2_rom_discovery_result mipsnes2_ps2_rom_discover_sidecar(
    const char *executable_path, char *path_out, size_t path_capacity)
{
    static const char *const extensions[] = { ".sfc", ".smc" };
    size_t index;

    if (path_out != NULL && path_capacity != 0U) {
        path_out[0] = '\0';
    }
    if (executable_path == NULL || executable_path[0] == '\0' ||
        path_out == NULL || path_capacity == 0U) {
        return MIPSNES2_PS2_ROM_DISCOVERY_INVALID_ARGUMENT;
    }
    for (index = 0U; index < sizeof(extensions) / sizeof(extensions[0]);
         ++index) {
        if (!sidecar_path(executable_path, extensions[index], path_out,
                          path_capacity)) {
            path_out[0] = '\0';
            return MIPSNES2_PS2_ROM_DISCOVERY_PATH_TOO_LONG;
        }
        if (readable_nonempty_file(path_out)) {
            return MIPSNES2_PS2_ROM_DISCOVERY_OK;
        }
    }
    path_out[0] = '\0';
    return MIPSNES2_PS2_ROM_DISCOVERY_NOT_FOUND;
}

mipsnes2_ps2_rom_discovery_result mipsnes2_ps2_rom_discover(
    const char *const *roots, size_t root_count,
    size_t max_entries_per_root, char *path_out, size_t path_capacity,
    mipsnes2_ps2_rom_discovery_report *report)
{
    mipsnes2_ps2_rom_discovery_report local_report;
    bool saw_overlong = false;
    size_t root_index;

    (void)memset(&local_report, 0, sizeof(local_report));
    if (path_out != NULL && path_capacity != 0U) {
        path_out[0] = '\0';
    }
    if (report != NULL) {
        *report = local_report;
    }
    if (roots == NULL || root_count == 0U ||
        root_count > MIPSNES2_PS2_ROM_DISCOVERY_MAX_ROOTS ||
        max_entries_per_root == 0U ||
        max_entries_per_root >
            MIPSNES2_PS2_ROM_DISCOVERY_MAX_ENTRIES_PER_ROOT ||
        path_out == NULL || path_capacity == 0U) {
        return MIPSNES2_PS2_ROM_DISCOVERY_INVALID_ARGUMENT;
    }

    for (root_index = 0U; root_index < root_count; ++root_index) {
        const char *root = roots[root_index];
        DIR *directory;
        size_t entries_in_root = 0U;
        bool selected = false;

        ++local_report.roots_attempted;
        if (root == NULL || root[0] == '\0') {
            continue;
        }
        directory = opendir(root);
        if (directory == NULL) {
            continue;
        }
        ++local_report.roots_opened;
        while (entries_in_root < max_entries_per_root) {
            struct dirent *entry = readdir(directory);
            char candidate[MIPSNES2_PS2_ROM_DISCOVERY_PATH_CAPACITY];

            if (entry == NULL) {
                break;
            }
            ++entries_in_root;
            ++local_report.entries_examined;
            if (!is_plain_name(entry->d_name) ||
                !has_rom_extension(entry->d_name)) {
                continue;
            }
            ++local_report.extension_matches;
            if (!join_path(root, entry->d_name, candidate,
                           sizeof(candidate)) ||
                strlen(candidate) + 1U > path_capacity) {
                ++local_report.overlong_matches;
                saw_overlong = true;
                continue;
            }
            if (!readable_nonempty_file(candidate)) {
                continue;
            }
            ++local_report.readable_matches;
            if (!selected || strcmp(candidate, path_out) < 0) {
                (void)memcpy(path_out, candidate, strlen(candidate) + 1U);
                selected = true;
            }
        }
        if (entries_in_root == max_entries_per_root) {
            local_report.entry_limit_reached = true;
        }
        (void)closedir(directory);
        if (selected) {
            if (report != NULL) {
                *report = local_report;
            }
            return MIPSNES2_PS2_ROM_DISCOVERY_OK;
        }
    }
    if (report != NULL) {
        *report = local_report;
    }
    return saw_overlong ? MIPSNES2_PS2_ROM_DISCOVERY_PATH_TOO_LONG :
        MIPSNES2_PS2_ROM_DISCOVERY_NOT_FOUND;
}
