/*
 * NES 游戏屏 presenter: nav_scr 生命周期 + 模拟器启停 + 帧驱动。
 * PC (sim) 与设备共用; ROM 来源差异通过 CONFIG_LVGL_ENV_SIMULATOR 分支:
 *   sim  -> 环境变量 NES_ROM_PATH / 默认 ./roms/game.nes, nes_load_file()
 *   设备 -> WS 推送的 staged ROM 优先 (gamepad_rom, 协议 §4.6),
 *           否则回退 nes_rom flash 分区 XIP, nes_load_rom()
 * 设备端支持热重启: tick 里检测 staged 代数变化 -> 停机重载新 ROM。
 */
#define LOG_TAG "nes_game"

#include <stdlib.h>

#include "lisa_ui.h"
#include "lisa_ui_log.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "nes.h"
#include "nes_game_port.h"
#if !defined(CONFIG_LVGL_ENV_SIMULATOR)
/* 设备端: 向 gamepad 中间件上报游戏运行态 (WS 手柄 welcome/state 用)。
 * sim 无此中间件, 用空实现替代。 */
#include "gamepad.h"
#define nes_game_notify_state(run, fps)  gamepad_notify_game_state((run), (fps))
#else
#define nes_game_notify_state(run, fps)  ((void)0)
#endif

#if !defined(CONFIG_LVGL_ENV_SIMULATOR)
/* 设备端: ROM 放在独立的 flash 分区 `nes_rom` (见 res/arcs-mini/partition_table.json),
 * 运行时由 CPU 直接 XIP 读取, **不占 PSRAM, 也不占 app 镜像体积**, 因此可放几百 KB 的大 ROM。
 *   flash 基址 0x30000000 (CMN_FLASH_REGION) + 分区偏移 0xE00000 = 0x30E00000
 * 通过 `adb_download.sh -S res/arcs-mini` (默认模式) 或 auto.sh 烧写该分区。 */
#define NES_ROM_FLASH_ADDR  0x30E00000UL
#endif

struct nes_game_nav_scr_data {
    lv_obj_t *view;
    lv_obj_t *img;
    lv_obj_t *status_label;
    lv_timer_t *tick_timer;
    nes_t *nes;
    bool rom_loaded;
    bool exited;        /* 模拟器已退出 (EXIT 显示过): tick 只留 ROM 热重启检测 */
#if !defined(CONFIG_LVGL_ENV_SIMULATOR)
    uint32_t rom_gen;   /* 当前加载的 staged ROM 代数 (0=flash 分区) */
#endif
};

#define NES_GAME_TICK_PERIOD_MS 8

static void nes_game_status_update(struct nes_game_nav_scr_data *scr_data, const char *text)
{
    if (scr_data && scr_data->status_label) {
        lv_label_set_text(scr_data->status_label, text);
        /* 文本变化后须重新对齐: lv_obj_align 是一次性计算, 不随内容宽度重排。
         * 之前这里误用 lv_obj_center, 把标签永久钉在屏幕正中 (FPS 一直挡在画面中央) */
        lv_obj_align(scr_data->status_label, LV_ALIGN_BOTTOM_MID, 0, -4);
    }
}

/* ------------------------------------------------------------------ */
/* 模拟器启停 (open / close / 热重启共用)                                */
/* ------------------------------------------------------------------ */
static void nes_game_emu_stop(struct nes_game_nav_scr_data *scr_data)
{
    nes_game_port_stop();
    nes_game_notify_state(false, 0);
    if (scr_data->nes) {
        nes_deinit(scr_data->nes);
        scr_data->nes = NULL;
    }
#if !defined(CONFIG_LVGL_ENV_SIMULATOR)
    /* 停机后核心不再读旧 staged 缓冲 (nes_load_rom 只取指针不拷贝),
     * retire 允许 gamepad_rom 释放被替换的旧代 */
    gamepad_rom_retire(scr_data->rom_gen);
#endif
    nes_game_port_detach();
    scr_data->rom_loaded = false;
    scr_data->exited = false;
}

static int nes_game_emu_start(struct nes_game_nav_scr_data *scr_data)
{
    scr_data->exited = false;
    nes_game_status_update(scr_data, "loading...");

    /* 初始化核心 */
    scr_data->nes = nes_init();
    if (!scr_data->nes) {
        nes_game_status_update(scr_data, "nes_init failed");
        return -1;
    }

#if defined(CONFIG_LVGL_ENV_SIMULATOR)
    /* sim: 本地文件 ROM */
    const char *rom_path = getenv("NES_ROM_PATH");
    if (!rom_path) {
        rom_path = "./roms/game.nes";
    }
    int load_ret = nes_load_file(scr_data->nes, rom_path);
    scr_data->rom_loaded = (load_ret == NES_OK);
    LISA_UI_LOGI("load %s ret=%d prg=%d chr=%d mapper=%d", rom_path, load_ret,
                 scr_data->nes->nes_rom.prg_rom_size, scr_data->nes->nes_rom.chr_rom_size,
                 scr_data->nes->nes_rom.mapper_number);
    if (!scr_data->rom_loaded) {
        nes_game_status_update(scr_data, "NO ROM (set NES_ROM_PATH)");
    }
#else
    /* 设备: WS 推送的 staged ROM 优先 (§4.6), 无推送回退 flash 分区 XIP */
    const uint8_t *rom = (const uint8_t *)NES_ROM_FLASH_ADDR;
    uint32_t rom_size = 0;
    uint32_t gen = gamepad_rom_acquire(&rom, &rom_size);
    if (gen > 0U) {
        scr_data->rom_gen = gen;
    } else {
        scr_data->rom_gen = 0U;
        rom = (const uint8_t *)NES_ROM_FLASH_ADDR;
    }
    int load_ret = nes_load_rom(scr_data->nes, rom);
    scr_data->rom_loaded = (load_ret == NES_OK);
    LISA_UI_LOGI("rom load (src=%s) ret=%d prg=%d chr=%d mapper=%d",
                 (gen > 0U) ? "ws-staged" : "flash",
                 load_ret,
                 scr_data->nes->nes_rom.prg_rom_size, scr_data->nes->nes_rom.chr_rom_size,
                 scr_data->nes->nes_rom.mapper_number);
    if (!scr_data->rom_loaded) {
        nes_game_status_update(scr_data, "ROM load failed (分区未烧录 / 推送数据损坏?)");
    }
#endif

    /* port 接入; ROM 就绪才启动模拟 (无 ROM 时 nes_run 会野指针) */
    nes_game_port_attach(scr_data->nes, scr_data->img);
    if (scr_data->rom_loaded && nes_game_port_start(scr_data->nes) != 0) {
        nes_game_status_update(scr_data, "start failed");
    }
    nes_game_notify_state(scr_data->rom_loaded, 0);

    return scr_data->rom_loaded ? 0 : -1;
}

#if !defined(CONFIG_LVGL_ENV_SIMULATOR)
/* 热重启: LVGL tick 上下文, 停旧 ROM 模拟 -> 加载新 staged ROM */
static void nes_game_reload(struct nes_game_nav_scr_data *scr_data)
{
    LISA_UI_LOGI("staged rom gen %u -> %u, hot reload",
                 (unsigned)scr_data->rom_gen, (unsigned)gamepad_rom_staged_gen());
    nes_game_emu_stop(scr_data);
    if (nes_game_emu_start(scr_data) != 0) {
        nes_game_status_update(scr_data, "ROM reload failed");
    }
}
#endif

static void nes_game_tick_cb(lv_timer_t *timer)
{
    struct nes_game_nav_scr_data *scr_data = (struct nes_game_nav_scr_data *)timer->user_data;

    if (!scr_data) {
        return;
    }

#if !defined(CONFIG_LVGL_ENV_SIMULATOR)
    /* 新 ROM 推送进来: 热重启 (优先于退出态判断, EXIT 后也能被新 ROM 唤醒) */
    if (gamepad_rom_staged_gen() != scr_data->rom_gen) {
        nes_game_reload(scr_data);
        return;
    }
    /* 手柄 Reset 键 (WS {"t":"cmd","c":"reset"}): 重载当前 ROM, 等价主机复位 */
    if (gamepad_consume_reset()) {
        nes_game_reload(scr_data);
        return;
    }
#endif

    nes_game_port_tick();

    if (nes_game_port_is_done()) {
        /* 只处理一次; 不删 tick_timer —— 保留热重启检测能力 */
        if (!scr_data->exited) {
            scr_data->exited = true;
            nes_game_port_stop();
            nes_game_notify_state(false, 0);
            nes_game_status_update(scr_data, "EXIT");
        }
        return;
    }

    static uint32_t fps_refresh_cnt = 0;
    if (++fps_refresh_cnt >= 250U) { /* 8ms * 250 = 2s */
        fps_refresh_cnt = 0;
        uint32_t fps = nes_game_port_get_logic_fps();
        nes_game_notify_state(scr_data->nes != NULL, fps);
        if (fps > 0U && scr_data->status_label) {
            char buf[32];
            snprintf(buf, sizeof(buf), "FPS %u", fps);
            nes_game_status_update(scr_data, buf);
        }
    }
}

static int nes_game_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    struct nes_game_nav_scr_data *scr_data = NULL;

    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    scr_data = lisa_ui_malloc(sizeof(struct nes_game_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(*scr_data));

    /* 全屏黑底容器 */
    scr_data->view = lv_obj_create(lv_scr_act());
    if (!scr_data->view) {
        lisa_ui_free(scr_data);
        return -1;
    }
    lv_obj_set_size(scr_data->view, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scr_data->view, lv_color_black(), 0);
    lv_obj_set_style_border_width(scr_data->view, 0, 0);
    lv_obj_set_style_pad_all(scr_data->view, 0, 0);
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_SCROLLABLE);

    /* NES 画面 */
    scr_data->img = lv_img_create(scr_data->view);

    /* 状态栏 (fps / 提示) */
    scr_data->status_label = lv_label_create(scr_data->view);
    lv_label_set_text(scr_data->status_label, "loading...");
    lv_obj_set_style_text_color(scr_data->status_label, lv_color_white(), 0);
    lv_obj_align(scr_data->status_label, LV_ALIGN_BOTTOM_MID, 0, -4);

    nes_game_emu_start(scr_data);

    scr_data->tick_timer = lv_timer_create(nes_game_tick_cb, NES_GAME_TICK_PERIOD_MS, scr_data);

    *data = scr_data;
    return 0;
}

static int nes_game_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct nes_game_nav_scr_data *scr_data = (struct nes_game_nav_scr_data *)data;

    LISA_UI_LOGD("nav scr show, id: %d", scr->unique_id);

    if (!scr_data || !scr_data->view) {
        return -1;
    }
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int nes_game_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    struct nes_game_nav_scr_data *scr_data = (struct nes_game_nav_scr_data *)data;

    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);

    if (!scr_data) {
        return 0;
    }

    if (scr_data->tick_timer) {
        lv_timer_del(scr_data->tick_timer);
        scr_data->tick_timer = NULL;
    }

    nes_game_emu_stop(scr_data);

    if (scr_data->view) {
        lv_obj_del(scr_data->view);
        scr_data->view = NULL;
    }

    lisa_ui_free(scr_data);
    return 0;
}

const struct lisa_ui_nav_scr nes_game_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_NES_GAME,
    .open = nes_game_nav_scr_open,
    .show = nes_game_nav_scr_show,
    .close = nes_game_nav_scr_close,
};
