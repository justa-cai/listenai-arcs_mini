/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "fs_utils.h"
#include "lsfs.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "fs_utils"
#include <lisa_log.h>

static bool str_ends_with_ci(const char *str, const char *suffix)
{
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    if (str_len < suf_len) {
        return false;
    }
    const char *end = str + str_len - suf_len;
    for (size_t i = 0; i < suf_len; i++) {
        if (tolower((unsigned char)end[i]) != tolower((unsigned char)suffix[i])) {
            return false;
        }
    }
    return true;
}

static void trim_trailing(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ')) {
        s[--len] = '\0';
    }
}

int fs_list_avi_files(const char *dir_path, fs_file_list_t *out)
{
    if (!dir_path || !out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    struct lsfs_dir_t dir;
    lsfs_dir_t_init(&dir);

    int ret = lsfs_opendir(&dir, dir_path);
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Cannot open dir: %s (ret=%d)", dir_path, ret);
        return -1;
    }

    struct lsfs_dirent entry;
    while (out->count < FS_MAX_ENTRIES) {
        ret = lsfs_readdir(&dir, &entry);
        if (ret != 0) {
            break;
        }
        if (entry.name[0] == '\0') {
            break;
        }
        if (entry.type != LSFS_DIR_ENTRY_FILE) {
            continue;
        }
        if (!str_ends_with_ci(entry.name, ".avi")) {
            continue;
        }

        strncpy(out->entries[out->count].name, entry.name, FS_MAX_FILENAME_LEN - 1);
        out->entries[out->count].name[FS_MAX_FILENAME_LEN - 1] = '\0';
        out->entries[out->count].size = (uint32_t)entry.size;
        out->count++;
    }

    lsfs_closedir(&dir);
    LISA_LOGI(LOG_TAG, "Found %d AVI files in %s", out->count, dir_path);
    return 0;
}

int fs_list_urls(const char *dir_path, fs_url_list_t *out)
{
    if (!dir_path || !out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    struct lsfs_dir_t dir;
    lsfs_dir_t_init(&dir);

    int ret = lsfs_opendir(&dir, dir_path);
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Cannot open dir: %s (ret=%d)", dir_path, ret);
        return -1;
    }

    struct lsfs_dirent entry;
    while (1) {
        ret = lsfs_readdir(&dir, &entry);
        if (ret != 0 || entry.name[0] == '\0') {
            break;
        }
        if (entry.type != LSFS_DIR_ENTRY_FILE) {
            continue;
        }
        if (!str_ends_with_ci(entry.name, ".ini")) {
            continue;
        }

        /* Read URLs from this INI file */
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry.name);

        FILE *fp = fopen(filepath, "r");
        if (!fp) {
            LISA_LOGW(LOG_TAG, "Cannot open: %s", filepath);
            continue;
        }

        char line[FS_MAX_URL_LEN];
        while (out->count < FS_MAX_ENTRIES && fgets(line, sizeof(line), fp)) {
            trim_trailing(line);
            /* Skip empty lines and comments */
            if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
                continue;
            }
            /* Must look like a URL */
            if (strncmp(line, "http://", 7) != 0 && strncmp(line, "https://", 8) != 0) {
                continue;
            }

            strncpy(out->entries[out->count].url, line, FS_MAX_URL_LEN - 1);
            out->entries[out->count].url[FS_MAX_URL_LEN - 1] = '\0';

            /* Label: truncate URL for display */
            const char *path_start = strstr(line + 8, "/");
            if (path_start) {
                const char *name = strrchr(path_start, '/');
                if (name && name[1] != '\0') {
                    strncpy(out->entries[out->count].label, name + 1, FS_MAX_FILENAME_LEN - 1);
                } else {
                    strncpy(out->entries[out->count].label, line, FS_MAX_FILENAME_LEN - 1);
                }
            } else {
                strncpy(out->entries[out->count].label, line, FS_MAX_FILENAME_LEN - 1);
            }
            out->entries[out->count].label[FS_MAX_FILENAME_LEN - 1] = '\0';

            out->count++;
        }

        fclose(fp);
    }

    lsfs_closedir(&dir);
    LISA_LOGI(LOG_TAG, "Found %d URLs in %s", out->count, dir_path);
    return 0;
}

int fs_read_wifi_config(const char *file_path, fs_wifi_config_t *out)
{
    if (!file_path || !out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        LISA_LOGW(LOG_TAG, "Cannot open WiFi config: %s", file_path);
        return -1;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        trim_trailing(line);

        if (strncmp(line, "WIFI_SSID=", 10) == 0) {
            strncpy(out->ssid, line + 10, sizeof(out->ssid) - 1);
        } else if (strncmp(line, "WIFI_PWD=", 9) == 0) {
            strncpy(out->pwd, line + 9, sizeof(out->pwd) - 1);
        }
    }

    fclose(fp);

    out->valid = (out->ssid[0] != '\0');
    if (out->valid) {
        LISA_LOGI(LOG_TAG, "WiFi config: SSID=%s", out->ssid);
    } else {
        LISA_LOGW(LOG_TAG, "WiFi config invalid (no SSID)");
    }

    return out->valid ? 0 : -1;
}
