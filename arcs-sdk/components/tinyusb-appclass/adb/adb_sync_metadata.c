#include "adb_sync_metadata.h"

#include "adb_utils.h"
#include "lsfs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct adb_sync_metadata_entry {
    char *path;
    uint32_t timestamp;
};

static size_t adb_sync_metadata_u32_len(uint32_t value)
{
    size_t len = 1u;

    while (value >= 10u) {
        value /= 10u;
        len++;
    }

    return len;
}

static int adb_sync_metadata_parse_u32(const char *text, uint32_t *value)
{
    unsigned long parsed;
    char *end = NULL;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return -1;
    }

    parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static void adb_sync_metadata_free_entries(struct adb_sync_metadata_entry *entries, size_t count)
{
    size_t i;

    if (entries == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) {
        ADB_FREE(entries[i].path);
    }
    ADB_FREE(entries);
}

static int adb_sync_metadata_normalize_relative_path(const char *path, char **normalized)
{
    const char *segment = NULL;
    const char *cursor = NULL;
    char *result = NULL;
    char *write_ptr = NULL;
    size_t path_len;

    if (path == NULL || normalized == NULL) {
        return ADB_SYNC_METADATA_INVALID_PATH;
    }
    if (path[0] == '/') {
        return ADB_SYNC_METADATA_BYPASS;
    }

    path_len = strlen(path);
    if (path_len == 0u) {
        return ADB_SYNC_METADATA_INVALID_PATH;
    }

    result = ADB_MALLOC(path_len + 1u);
    if (result == NULL) {
        return ADB_SYNC_METADATA_NO_MEMORY;
    }

    write_ptr = result;
    segment = path;
    cursor = path;

    while (true) {
        char ch = *cursor;
        if (ch == '/' || ch == '\0') {
            size_t segment_len = (size_t)(cursor - segment);

            if (segment_len == 0u) {
                ADB_FREE(result);
                return ADB_SYNC_METADATA_INVALID_PATH;
            }
            if ((segment_len == 1u && segment[0] == '.') ||
                (segment_len == 2u && segment[0] == '.' && segment[1] == '.')) {
                ADB_FREE(result);
                return ADB_SYNC_METADATA_INVALID_PATH;
            }

            if (write_ptr != result) {
                *write_ptr++ = '/';
            }
            memcpy(write_ptr, segment, segment_len);
            write_ptr += segment_len;

            if (ch == '\0') {
                break;
            }

            segment = cursor + 1;
        } else if ((unsigned char)ch < 0x20u || ch == '\x7f' || ch == '\\') {
            ADB_FREE(result);
            return ADB_SYNC_METADATA_INVALID_PATH;
        }
        cursor++;
    }

    *write_ptr = '\0';
    *normalized = result;
    return ADB_SYNC_METADATA_OK;
}

static int adb_sync_metadata_load_sidecar(uint8_t **content, size_t *content_size)
{
    struct lsfs_dirent entry = {0};
    struct lsfs_file_t file;
    uint8_t *buffer = NULL;
    size_t offset = 0u;
    int r;

    if (content == NULL || content_size == NULL) {
        return ADB_SYNC_METADATA_INVALID_PATH;
    }

    r = lsfs_stat(ADB_SYNC_METADATA_SIDECAR_PATH, &entry);
    if (r != 0) {
        return ADB_SYNC_METADATA_NOT_FOUND;
    }

    buffer = ADB_MALLOC((size_t)entry.size + 1u);
    if (buffer == NULL) {
        return ADB_SYNC_METADATA_NO_MEMORY;
    }

    lsfs_file_t_init(&file);
    r = lsfs_open(&file, ADB_SYNC_METADATA_SIDECAR_PATH, LSFS_O_READ);
    if (r != 0) {
        ADB_FREE(buffer);
        return ADB_SYNC_METADATA_IO_ERROR;
    }

    while (offset < entry.size) {
        ssize_t read_size = lsfs_read(&file, buffer + offset, (size_t)entry.size - offset);
        if (read_size <= 0) {
            lsfs_close(&file);
            ADB_FREE(buffer);
            return ADB_SYNC_METADATA_IO_ERROR;
        }
        offset += (size_t)read_size;
    }

    if (lsfs_close(&file) != 0) {
        ADB_FREE(buffer);
        return ADB_SYNC_METADATA_IO_ERROR;
    }

    buffer[entry.size] = '\0';
    *content = buffer;
    *content_size = (size_t)entry.size;
    return ADB_SYNC_METADATA_OK;
}

static int adb_sync_metadata_parse_entries(uint8_t *content, size_t content_size,
                                           struct adb_sync_metadata_entry **entries_out, size_t *count_out)
{
    struct adb_sync_metadata_entry *entries = NULL;
    size_t capacity = 0u;
    size_t count = 0u;
    uint8_t *cursor = content;
    uint8_t *end = content + content_size;

    if (entries_out == NULL || count_out == NULL) {
        return ADB_SYNC_METADATA_INVALID_PATH;
    }

    while (cursor < end) {
        uint8_t *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        uint8_t *tab = NULL;
        char *normalized_path = NULL;
        uint32_t timestamp = 0u;
        int status;

        if (line_end == NULL) {
            line_end = end;
        }
        if (line_end == cursor) {
            adb_sync_metadata_free_entries(entries, count);
            return ADB_SYNC_METADATA_MALFORMED_RECORD;
        }

        tab = memchr(cursor, '\t', (size_t)(line_end - cursor));
        if (tab == NULL || tab == cursor || tab == (line_end - 1)) {
            adb_sync_metadata_free_entries(entries, count);
            return ADB_SYNC_METADATA_MALFORMED_RECORD;
        }

        *tab = '\0';
        status = adb_sync_metadata_parse_u32((char *)cursor, &timestamp);
        if (status != 0) {
            adb_sync_metadata_free_entries(entries, count);
            return ADB_SYNC_METADATA_MALFORMED_RECORD;
        }

        *line_end = '\0';
        status = adb_sync_metadata_normalize_relative_path((char *)(tab + 1), &normalized_path);
        if (status != ADB_SYNC_METADATA_OK) {
            adb_sync_metadata_free_entries(entries, count);
            return ADB_SYNC_METADATA_MALFORMED_RECORD;
        }

        if (count == capacity) {
            size_t next_capacity = capacity == 0u ? 4u : capacity * 2u;
            struct adb_sync_metadata_entry *next_entries = ADB_MALLOC(next_capacity * sizeof(*next_entries));
            if (next_entries == NULL) {
                ADB_FREE(normalized_path);
                adb_sync_metadata_free_entries(entries, count);
                return ADB_SYNC_METADATA_NO_MEMORY;
            }
            if (entries != NULL) {
                memcpy(next_entries, entries, count * sizeof(*entries));
                ADB_FREE(entries);
            }
            entries = next_entries;
            capacity = next_capacity;
        }

        entries[count].path = normalized_path;
        entries[count].timestamp = timestamp;
        count++;
        cursor = line_end + 1;
    }

    *entries_out = entries;
    *count_out = count;
    return ADB_SYNC_METADATA_OK;
}

static int adb_sync_metadata_write_entries(const struct adb_sync_metadata_entry *entries, size_t count)
{
    struct lsfs_file_t file;
    uint8_t *buffer = NULL;
    size_t offset = 0u;
    size_t total_size = 0u;
    size_t i;
    int r;

    for (i = 0u; i < count; ++i) {
        total_size += adb_sync_metadata_u32_len(entries[i].timestamp) + 1u + strlen(entries[i].path) + 1u;
    }

    buffer = ADB_MALLOC(total_size + 1u);
    if (buffer == NULL) {
        return ADB_SYNC_METADATA_NO_MEMORY;
    }

    for (i = 0u; i < count; ++i) {
        int written = snprintf((char *)buffer + offset, total_size + 1u - offset, "%u\t%s\n",
                               entries[i].timestamp, entries[i].path);
        if (written < 0 || (size_t)written >= (total_size + 1u - offset)) {
            ADB_FREE(buffer);
            return ADB_SYNC_METADATA_IO_ERROR;
        }
        offset += (size_t)written;
    }

    lsfs_file_t_init(&file);
    r = lsfs_open(&file, ADB_SYNC_METADATA_SIDECAR_PATH, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_WRITE);
    if (r != 0) {
        ADB_FREE(buffer);
        return ADB_SYNC_METADATA_IO_ERROR;
    }

    if (total_size != 0u) {
        ssize_t write_size = lsfs_write(&file, buffer, total_size);
        if (write_size != (ssize_t)total_size) {
            lsfs_close(&file);
            ADB_FREE(buffer);
            return ADB_SYNC_METADATA_IO_ERROR;
        }
    }

    if (lsfs_close(&file) != 0) {
        ADB_FREE(buffer);
        return ADB_SYNC_METADATA_IO_ERROR;
    }

    ADB_FREE(buffer);
    return ADB_SYNC_METADATA_OK;
}

const char *adb_sync_metadata_status_string(int status)
{
    switch (status) {
    case ADB_SYNC_METADATA_OK:
        return "ok";
    case ADB_SYNC_METADATA_NOT_FOUND:
        return "not-found";
    case ADB_SYNC_METADATA_BYPASS:
        return "bypass";
    case ADB_SYNC_METADATA_INVALID_PATH:
        return "invalid-path";
    case ADB_SYNC_METADATA_IO_ERROR:
        return "io-error";
    case ADB_SYNC_METADATA_MALFORMED_RECORD:
        return "malformed-record";
    case ADB_SYNC_METADATA_NO_MEMORY:
        return "no-memory";
    default:
        return "unknown";
    }
}

int adb_sync_metadata_build_full_path(const char *path, char **full_path)
{
    char *normalized_path = NULL;
    char *resolved_path = NULL;
    size_t root_len;
    size_t normalized_len;
    int status;

    if (path == NULL || full_path == NULL) {
        return ADB_SYNC_METADATA_INVALID_PATH;
    }

    if (path[0] == '/') {
        size_t absolute_len = strlen(path) + 1u;
        resolved_path = ADB_MALLOC(absolute_len);
        if (resolved_path == NULL) {
            return ADB_SYNC_METADATA_NO_MEMORY;
        }
        memcpy(resolved_path, path, absolute_len);
        *full_path = resolved_path;
        return ADB_SYNC_METADATA_OK;
    }

    status = adb_sync_metadata_normalize_relative_path(path, &normalized_path);
    if (status != ADB_SYNC_METADATA_OK) {
        return status;
    }

    root_len = strlen(ADB_SYNC_METADATA_ROOT);
    normalized_len = strlen(normalized_path);
    resolved_path = ADB_MALLOC(root_len + normalized_len + 1u);
    if (resolved_path == NULL) {
        ADB_FREE(normalized_path);
        return ADB_SYNC_METADATA_NO_MEMORY;
    }

    memcpy(resolved_path, ADB_SYNC_METADATA_ROOT, root_len);
    memcpy(resolved_path + root_len, normalized_path, normalized_len + 1u);
    ADB_FREE(normalized_path);

    *full_path = resolved_path;
    return ADB_SYNC_METADATA_OK;
}

int adb_sync_metadata_get_timestamp(const char *path, uint32_t *timestamp)
{
    uint8_t *content = NULL;
    size_t content_size = 0u;
    struct adb_sync_metadata_entry *entries = NULL;
    char *normalized_path = NULL;
    size_t count = 0u;
    size_t i;
    int status;

    status = adb_sync_metadata_normalize_relative_path(path, &normalized_path);
    if (status != ADB_SYNC_METADATA_OK) {
        return status;
    }

    status = adb_sync_metadata_load_sidecar(&content, &content_size);
    if (status == ADB_SYNC_METADATA_NOT_FOUND) {
        ADB_FREE(normalized_path);
        return ADB_SYNC_METADATA_NOT_FOUND;
    }
    if (status != ADB_SYNC_METADATA_OK) {
        ADB_FREE(normalized_path);
        return status;
    }

    status = adb_sync_metadata_parse_entries(content, content_size, &entries, &count);
    ADB_FREE(content);
    if (status != ADB_SYNC_METADATA_OK) {
        ADB_FREE(normalized_path);
        return status;
    }

    for (i = 0u; i < count; ++i) {
        if (strcmp(entries[i].path, normalized_path) == 0) {
            if (timestamp != NULL) {
                *timestamp = entries[i].timestamp;
            }
            ADB_FREE(normalized_path);
            adb_sync_metadata_free_entries(entries, count);
            return ADB_SYNC_METADATA_OK;
        }
    }

    ADB_FREE(normalized_path);
    adb_sync_metadata_free_entries(entries, count);
    return ADB_SYNC_METADATA_NOT_FOUND;
}

int adb_sync_metadata_record_timestamp(const char *path, uint32_t timestamp)
{
    uint8_t *content = NULL;
    size_t content_size = 0u;
    struct adb_sync_metadata_entry *entries = NULL;
    struct adb_sync_metadata_entry *next_entries = NULL;
    size_t count = 0u;
    size_t next_count = 0u;
    size_t i;
    char *normalized_path = NULL;
    const char *match_path = NULL;
    bool replaced = false;
    int status;

    status = adb_sync_metadata_normalize_relative_path(path, &normalized_path);
    if (status != ADB_SYNC_METADATA_OK) {
        return status;
    }
    match_path = normalized_path;

    status = adb_sync_metadata_load_sidecar(&content, &content_size);
    if (status != ADB_SYNC_METADATA_NOT_FOUND && status != ADB_SYNC_METADATA_OK) {
        ADB_FREE(normalized_path);
        return status;
    }

    if (status == ADB_SYNC_METADATA_OK) {
        status = adb_sync_metadata_parse_entries(content, content_size, &entries, &count);
        ADB_FREE(content);
        if (status != ADB_SYNC_METADATA_OK) {
            ADB_FREE(normalized_path);
            return status;
        }
    }

    next_entries = ADB_MALLOC((count + 1u) * sizeof(*next_entries));
    if (next_entries == NULL) {
        ADB_FREE(normalized_path);
        adb_sync_metadata_free_entries(entries, count);
        return ADB_SYNC_METADATA_NO_MEMORY;
    }

    for (i = 0u; i < count; ++i) {
        if (strcmp(entries[i].path, match_path) == 0) {
            if (!replaced) {
                next_entries[next_count].path = normalized_path;
                next_entries[next_count].timestamp = timestamp;
                next_count++;
                normalized_path = NULL;
                replaced = true;
            }
            ADB_FREE(entries[i].path);
            continue;
        }

        next_entries[next_count] = entries[i];
        next_count++;
    }

    if (!replaced) {
        next_entries[next_count].path = normalized_path;
        next_entries[next_count].timestamp = timestamp;
        next_count++;
        normalized_path = NULL;
    }

    ADB_FREE(entries);

    status = adb_sync_metadata_write_entries(next_entries, next_count);
    if (normalized_path != NULL) {
        ADB_FREE(normalized_path);
    }
    adb_sync_metadata_free_entries(next_entries, next_count);
    return status;
}
