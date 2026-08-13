/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "game_nes_input.h"
#include "game_nes_usb_keyboard.h"
#include "nes.h"
#include "lsfs.h"
#include "lvfs.h"
#include "lvgl.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"
#include "lisa_thread.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "game_nes"
#include <lisa_log.h>

#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/"SDMMC_DEVICE
#define GAME_NES_ROMS_DIR SDMMC_MOUNT_POINT"/roms"
#define GAME_NES_MAX_ROMS 128
#define GAME_NES_MAX_ROM_PATH_LEN 256

typedef struct {
    char **paths;
    size_t count;
} game_nes_rom_list_t;

typedef struct {
    volatile bool selected;
    char selected_path[GAME_NES_MAX_ROM_PATH_LEN];
} game_nes_rom_pick_ctx_t;

typedef struct {
    game_nes_rom_pick_ctx_t *pick_ctx;
    const char *path;
} game_nes_rom_btn_ctx_t;

typedef struct {
    volatile bool retry;
} game_nes_prompt_ctx_t;

static struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};
static bool g_sdmmc_mounted = false;
static bool g_fs_stack_inited = false;

static bool game_nes_has_suffix_ci(const char *name, const char *suffix)
{
    if (!name || !suffix) {
        return false;
    }

    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    if (name_len < suffix_len) {
        return false;
    }

    const char *start = name + (name_len - suffix_len);
    for (size_t i = 0; i < suffix_len; i++) {
        char c1 = start[i];
        char c2 = suffix[i];
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = (char)(c1 - 'A' + 'a');
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = (char)(c2 - 'A' + 'a');
        }
        if (c1 != c2) {
            return false;
        }
    }

    return true;
}

static int game_nes_ci_strcmp(const char *a, const char *b)
{
    if (!a && !b) {
        return 0;
    }
    if (!a) {
        return -1;
    }
    if (!b) {
        return 1;
    }

    while (*a != '\0' && *b != '\0') {
        char c1 = *a;
        char c2 = *b;
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = (char)(c1 - 'A' + 'a');
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = (char)(c2 - 'A' + 'a');
        }
        if (c1 != c2) {
            return (int)((unsigned char)c1 - (unsigned char)c2);
        }
        a++;
        b++;
    }

    return (int)((unsigned char)(*a) - (unsigned char)(*b));
}

static int game_nes_rom_path_cmp(const void *a, const void *b)
{
    const char *const *pa = (const char *const *)a;
    const char *const *pb = (const char *const *)b;
    return game_nes_ci_strcmp(*pa, *pb);
}

static void game_nes_free_rom_list(game_nes_rom_list_t *rom_list)
{
    if (!rom_list || !rom_list->paths) {
        return;
    }

    for (size_t i = 0; i < rom_list->count; i++) {
        free(rom_list->paths[i]);
        rom_list->paths[i] = NULL;
    }
    free(rom_list->paths);
    rom_list->paths = NULL;
    rom_list->count = 0;
}

static int game_nes_scan_roms(game_nes_rom_list_t *rom_list)
{
    if (!rom_list) {
        return -1;
    }

    memset(rom_list, 0, sizeof(*rom_list));
    rom_list->paths = (char **)calloc(GAME_NES_MAX_ROMS, sizeof(char *));
    if (!rom_list->paths) {
        LISA_LOGE(LOG_TAG, "No memory for ROM list");
        return -1;
    }

    DIR *dir = opendir(GAME_NES_ROMS_DIR);
    if (!dir) {
        LISA_LOGE(LOG_TAG, "Cannot open ROM dir: %s", GAME_NES_ROMS_DIR);
        game_nes_free_rom_list(rom_list);
        return -1;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (!entry->d_name || entry->d_name[0] == '\0') {
            continue;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!game_nes_has_suffix_ci(entry->d_name, ".nes")) {
            continue;
        }
        if (rom_list->count >= GAME_NES_MAX_ROMS) {
            LISA_LOGW(LOG_TAG, "ROM count exceeds limit(%d), truncating", GAME_NES_MAX_ROMS);
            break;
        }

        char full_path[GAME_NES_MAX_ROM_PATH_LEN] = {0};
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", GAME_NES_ROMS_DIR, entry->d_name);
        if (len <= 0 || (size_t)len >= sizeof(full_path)) {
            LISA_LOGW(LOG_TAG, "Skip too-long path: %s", entry->d_name);
            continue;
        }

        size_t alloc_size = (size_t)len + 1U;
        char *path = (char *)malloc(alloc_size);
        if (!path) {
            LISA_LOGE(LOG_TAG, "No memory for ROM path");
            closedir(dir);
            game_nes_free_rom_list(rom_list);
            return -1;
        }
        memcpy(path, full_path, alloc_size);
        rom_list->paths[rom_list->count++] = path;
    }

    closedir(dir);

    if (rom_list->count == 0U) {
        LISA_LOGW(LOG_TAG, "No .nes file under %s", GAME_NES_ROMS_DIR);
        game_nes_free_rom_list(rom_list);
        return -1;
    }

    qsort(rom_list->paths, rom_list->count, sizeof(char *), game_nes_rom_path_cmp);
    return 0;
}

static void game_nes_rom_select_event_cb(lv_event_t *e)
{
    game_nes_rom_btn_ctx_t *btn_ctx = (game_nes_rom_btn_ctx_t *)lv_event_get_user_data(e);
    if (!btn_ctx || !btn_ctx->pick_ctx || !btn_ctx->path) {
        return;
    }

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    game_nes_rom_pick_ctx_t *pick_ctx = btn_ctx->pick_ctx;
    strncpy(pick_ctx->selected_path, btn_ctx->path, sizeof(pick_ctx->selected_path) - 1U);
    pick_ctx->selected_path[sizeof(pick_ctx->selected_path) - 1U] = '\0';
    pick_ctx->selected = true;
}

static void game_nes_prompt_retry_event_cb(lv_event_t *e)
{
    game_nes_prompt_ctx_t *ctx = (game_nes_prompt_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ctx->retry = true;
}

static void game_nes_wait_retry_prompt(const char *title, const char *line1, const char *line2)
{
    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
        return;
    }

    lv_coord_t screen_w = lv_obj_get_width(screen);
    lv_coord_t screen_h = lv_obj_get_height(screen);
    if (screen_w <= 0) {
        screen_w = 320;
    }
    if (screen_h <= 0) {
        screen_h = 240;
    }

    game_nes_prompt_ctx_t prompt_ctx = {0};

    lv_obj_t *overlay = lv_obj_create(screen);
    lv_obj_set_size(overlay, screen_w, screen_h);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 8, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);

    lv_obj_t *title_label = lv_label_create(overlay);
    lv_label_set_text(title_label, title ? title : "");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -58);

    lv_obj_t *line1_label = lv_label_create(overlay);
    lv_obj_set_width(line1_label, screen_w - 24);
    lv_label_set_long_mode(line1_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(line1_label, line1 ? line1 : "");
    lv_obj_set_style_text_color(line1_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(line1_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *line2_label = lv_label_create(overlay);
    lv_obj_set_width(line2_label, screen_w - 24);
    lv_label_set_long_mode(line2_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(line2_label, line2 ? line2 : "");
    lv_obj_set_style_text_color(line2_label, lv_color_make(0xc0, 0xc0, 0xc0), 0);
    lv_obj_set_style_text_align(line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(line2_label, LV_ALIGN_CENTER, 0, 12);

    lv_obj_t *retry_btn = lv_btn_create(overlay);
    lv_obj_set_size(retry_btn, 104, 38);
    lv_obj_align(retry_btn, LV_ALIGN_CENTER, 0, 64);
    lv_obj_add_event_cb(retry_btn, game_nes_prompt_retry_event_cb, LV_EVENT_CLICKED, &prompt_ctx);

    lv_obj_t *retry_label = lv_label_create(retry_btn);
    lv_label_set_text(retry_label, "Retry");
    lv_obj_center(retry_label);

    while (!prompt_ctx.retry) {
        (void)lv_task_handler();
        lisa_thread_mdelay(20);
    }

    lv_obj_del(overlay);
}

static int game_nes_pick_rom_by_lvgl(const game_nes_rom_list_t *rom_list,
                                     char *selected_path,
                                     size_t selected_path_len)
{
    if (!rom_list || !rom_list->paths || rom_list->count == 0U || !selected_path || selected_path_len == 0U) {
        return -1;
    }

    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
        return -1;
    }

    game_nes_rom_pick_ctx_t pick_ctx = {0};
    game_nes_rom_btn_ctx_t *btn_ctx_list =
        (game_nes_rom_btn_ctx_t *)calloc(rom_list->count, sizeof(game_nes_rom_btn_ctx_t));
    if (!btn_ctx_list) {
        LISA_LOGE(LOG_TAG, "No memory for LVGL ROM buttons");
        return -1;
    }

    lv_coord_t screen_w = lv_obj_get_width(screen);
    lv_coord_t screen_h = lv_obj_get_height(screen);

    lv_obj_t *overlay = lv_obj_create(screen);
    lv_obj_set_size(overlay, screen_w, screen_h);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 6, 0);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text(title, "Select NES ROM");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);

    lv_obj_t *tip = lv_label_create(overlay);
    lv_label_set_text(tip, "Tap a file under /SD:/roms/");
    lv_obj_align(tip, LV_ALIGN_BOTTOM_LEFT, 2, 0);

    lv_obj_t *rom_list_view = lv_list_create(overlay);
    lv_obj_set_size(rom_list_view, screen_w - 4, screen_h - 34);
    lv_obj_align(rom_list_view, LV_ALIGN_TOP_MID, 0, 16);

    for (size_t i = 0; i < rom_list->count; i++) {
        const char *path = rom_list->paths[i];
        const char *name = strrchr(path, '/');
        name = (name && *(name + 1) != '\0') ? (name + 1) : path;

        lv_obj_t *item = lv_list_add_btn(rom_list_view, NULL, name);
        btn_ctx_list[i].pick_ctx = &pick_ctx;
        btn_ctx_list[i].path = path;
        lv_obj_add_event_cb(item, game_nes_rom_select_event_cb, LV_EVENT_CLICKED, &btn_ctx_list[i]);
    }

    while (!pick_ctx.selected) {
        (void)lv_task_handler();
        lisa_thread_mdelay(10);
    }

    strncpy(selected_path, pick_ctx.selected_path, selected_path_len - 1U);
    selected_path[selected_path_len - 1U] = '\0';

    lv_obj_del(overlay);
    free(btn_ctx_list);
    return 0;
}

static int game_nes_select_rom_path(char *selected_path, size_t selected_path_len)
{
    game_nes_rom_list_t rom_list = {0};
    int ret = game_nes_scan_roms(&rom_list);
    if (ret != 0) {
        return ret;
    }

#if NES_DIRECT_LCD
    /* Direct LCD spike: prefer Castlevania.nes for measurement consistency
     * with previous baselines; fall back to paths[0] if unavailable. */
    const char *picked = NULL;
    for (size_t i = 0; i < rom_list.count; i++) {
        const char *name = strrchr(rom_list.paths[i], '/');
        name = (name && *(name + 1) != '\0') ? (name + 1) : rom_list.paths[i];
        if (game_nes_ci_strcmp(name, "Castlevania.nes") == 0) {
            picked = rom_list.paths[i];
            break;
        }
    }
    if (!picked) {
        picked = rom_list.paths[0];
    }
    strncpy(selected_path, picked, selected_path_len - 1U);
    selected_path[selected_path_len - 1U] = '\0';
    LISA_LOGI(LOG_TAG, "DIRECT_LCD: auto-selected ROM: %s", selected_path);
    ret = 0;
#else
    ret = game_nes_pick_rom_by_lvgl(&rom_list, selected_path, selected_path_len);
#endif
    game_nes_free_rom_list(&rom_list);
    return ret;
}

void game_nes_poll_joypad(nes_t *nes)
{
    if (!nes) {
        return;
    }
    if (game_nes_consume_rom_list_request()) {
        nes->nes_quit = 1;
        return;
    }
    nes->nes_cpu.joypad.joypad = game_nes_get_joypad_state();
}

static int game_nes_fs_init(void)
{
    int ret;

    if (g_sdmmc_mounted) {
        return 0;
    }

    ret = lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "SD/MMC card not ready: %d", ret);
        return ret;
    }

    disk_init(NULL);

    if (!g_fs_stack_inited) {
        lvfs_init();
        lsfs_init();
        g_fs_stack_inited = true;
    }

    ret = lsfs_mount(&sdmmc_mnt);
    if (ret != 0) {
        LISA_LOGI(LOG_TAG, "Mount failed, formatting SD card...");
        ret = lsfs_mkfs(LSFS_FATFS, SDMMC_DEVICE, NULL, 0);
        if (ret == 0) {
            ret = lsfs_mount(&sdmmc_mnt);
        }
    }

    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to mount filesystem: %d", ret);
        return ret;
    }

    LISA_LOGI(LOG_TAG, "Mounted %s", SDMMC_MOUNT_POINT);
    g_sdmmc_mounted = true;
    return 0;
}

static void game_nes_fs_deinit(void)
{
    if (!g_sdmmc_mounted) {
        return;
    }

    if (lsfs_unmount(&sdmmc_mnt) == 0) {
        g_sdmmc_mounted = false;
    }
}

static void game_nes_wait_for_sd_card(void)
{
    while (game_nes_fs_init() != 0) {
        game_nes_wait_retry_prompt("Insert SD Card",
                                   "Insert a FAT SD card with .nes files in /roms/.",
                                   "Then tap Retry.");
    }
}

static void game_nes_wait_for_rom_retry(const char *rom_path)
{
    char detail[GAME_NES_MAX_ROM_PATH_LEN + 32];
    snprintf(detail, sizeof(detail), "Path: %s", rom_path ? rom_path : "(null)");
    game_nes_wait_retry_prompt("Load ROM Failed", detail, "Tap Retry to open ROM list.");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LISA_LOGI(LOG_TAG, "Game NES sample start");

    game_nes_usb_keyboard_init();

    nes_t *nes = nes_init();
    if (!nes) {
        LISA_LOGE(LOG_TAG, "nes_init failed");
        return -1;
    }

#if (NES_USE_FS == 1)
    while (true) {
        char selected_rom_path[GAME_NES_MAX_ROM_PATH_LEN] = {0};
        const char *rom_path = NULL;

        game_nes_wait_for_sd_card();

        if (game_nes_select_rom_path(selected_rom_path, sizeof(selected_rom_path)) == 0) {
            rom_path = selected_rom_path;
            LISA_LOGI(LOG_TAG, "Selected ROM: %s", rom_path);
        } else {
            rom_path = CONFIG_GAME_NES_ROM_PATH;
            LISA_LOGW(LOG_TAG, "Use fallback ROM path from Kconfig: %s", rom_path ? rom_path : "(null)");
        }

        if (!rom_path || strlen(rom_path) == 0) {
            LISA_LOGE(LOG_TAG, "ROM path is empty");
            game_nes_wait_for_rom_retry(rom_path);
            continue;
        }

        int ret = nes_load_file(nes, rom_path);
        if (ret != NES_OK) {
            LISA_LOGE(LOG_TAG, "nes_load_file failed: %s", rom_path);
            game_nes_wait_for_rom_retry(rom_path);
            continue;
        }

        LISA_LOGI(LOG_TAG, "ROM loaded: %s", rom_path);
        game_nes_clear_rom_list_request();
        nes->nes_quit = 0;
#if (NES_FRAME_SKIP != 0) || (CONFIG_NES_DRAW_FPS_TARGET > 0)
        nes->nes_frame_skip_count = 0;
#endif
        nes_run(nes);
        nes_unload_file(nes);
        LISA_LOGI(LOG_TAG, "Returned to ROM list");
    }
#else
    LISA_LOGE(LOG_TAG, "NES_USE_FS is disabled, cannot load ROM from file");
#endif

    nes_deinit(nes);
    game_nes_fs_deinit();
    return 0;
}
