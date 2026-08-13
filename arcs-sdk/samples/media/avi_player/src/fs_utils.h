/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stdint.h>
#include <stdbool.h>

#define FS_MAX_FILENAME_LEN  64
#define FS_MAX_ENTRIES       32
#define FS_MAX_URL_LEN       256

typedef struct {
    char name[FS_MAX_FILENAME_LEN];
    uint32_t size;
} fs_file_entry_t;

typedef struct {
    fs_file_entry_t entries[FS_MAX_ENTRIES];
    int count;
} fs_file_list_t;

typedef struct {
    char url[FS_MAX_URL_LEN];
    char label[FS_MAX_FILENAME_LEN]; /* display name */
} fs_url_entry_t;

typedef struct {
    fs_url_entry_t entries[FS_MAX_ENTRIES];
    int count;
} fs_url_list_t;

typedef struct {
    char ssid[64];
    char pwd[64];
    bool valid;
} fs_wifi_config_t;

/**
 * List .avi files in a directory.
 * @param dir_path  e.g. "/SD:/Video"
 * @param out       populated file list
 * @return 0 on success, -1 on error
 */
int fs_list_avi_files(const char *dir_path, fs_file_list_t *out);

/**
 * List URLs from .ini files in a directory.
 * Each .ini file contains one URL per line.
 * @param dir_path  e.g. "/SD:/URL"
 * @param out       populated URL list
 * @return 0 on success, -1 on error
 */
int fs_list_urls(const char *dir_path, fs_url_list_t *out);

/**
 * Read WiFi config from an INI file.
 * Expected format: WIFI_SSID=xxx and WIFI_PWD=xxx on separate lines.
 * @param file_path  e.g. "/SD:/WiFi/wifi.ini"
 * @param out        populated config
 * @return 0 on success, -1 on error
 */
int fs_read_wifi_config(const char *file_path, fs_wifi_config_t *out);

#endif /* FS_UTILS_H */
