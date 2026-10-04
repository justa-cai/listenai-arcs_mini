/*
 * pet_ui.c - full-screen pet page (default screen)
 *
 * Layout on the 240x240 panel:
 *   y  0..26   status: four stat groups (label + lv_bar)
 *   y  32..48  stage / day line (chinese_16)
 *   y  40..200 160x160 character canvas (transparent background)
 *   y 214..232 action hint row: 喂食 清洁 玩耍 睡觉 吃药 状态 (voice commands)
 *
 * Two lv_timers run while the page is shown:
 *   anim_timer (66 ms): advance the draw context and redraw the canvas
 *   core_timer (1 s): pet_core_tick(), sync widgets, consume event bits
 *                     (reaction animations + beeps + reminder toasts)
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_ui_fonts.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lvgl.h"
#include "models/model_voice.h"

#include "pet_buzzer.h"
#include "pet_core.h"
#include "pet_draw.h"
#include "pet_ui.h"
#include "pet_voice.h"

#define TAG "pet"

#define PET_SCREEN_W 240
#define PET_SCREEN_H 240

#define PET_ANIM_PERIOD_MS 66
#define PET_CORE_PERIOD_MS 1000
#define PET_TOAST_MS 2000

/* stat bar groups */
#define PET_STAT_GROUP_W 56
#define PET_STAT_BAR_W 44

/* action hint row */
#define PET_HINT_NUM 6

typedef struct {
    lv_obj_t *root;
    lv_timer_t *anim_timer;
    lv_timer_t *core_timer;

    lv_obj_t *stat_label[4];
    lv_obj_t *stat_bar[4];
    uint8_t stat_val[4];

    lv_obj_t *stage_label;
    char stage_text[40];
    pet_stage_t last_stage;
    uint16_t last_age_days;

    lv_obj_t *canvas;
    uint8_t *canvas_buf;
    pet_draw_ctx_t draw_ctx;
    pet_snapshot_t last_snap;

    lv_obj_t *hint[PET_HINT_NUM];
    int hint_sel;

    lv_obj_t *toast;
    lv_timer_t *toast_timer;
} pet_page_t;

static pet_page_t *s_page;

static const char *const k_stat_names[4] = {"饱", "乐", "洁", "力"};
static const uint16_t k_stat_colors[4] = {
    (0xF8 << 8) | (0xA0 << 3) | (0x2A >> 3), /* satiety orange */
    (0xF8 << 8) | (0xD0 << 3) | (0x30 >> 3), /* happy yellow */
    (0x2A << 8) | (0x90 << 3) | (0xE8 >> 3), /* clean blue */
    (0x50 << 8) | (0xC8 << 3) | (0x50 >> 3), /* energy green */
};
static const uint8_t k_hint_actions[PET_HINT_NUM] = {
    PET_ACTION_FEED, PET_ACTION_CLEAN, PET_ACTION_PLAY,
    PET_ACTION_LIGHT, PET_ACTION_MEDICINE, PET_ACTION_STATUS,
};

/* ---------------- toast ---------------- */

static void pet_toast_timer_cb(lv_timer_t *t)
{
    pet_page_t *page = t ? t->user_data : NULL;
    if (page && page->toast) {
        lv_obj_add_flag(page->toast, LV_OBJ_FLAG_HIDDEN);
    }
    if (t) {
        lv_timer_del(t);
    }
    if (page) {
        page->toast_timer = NULL;
    }
}

void pet_ui_toast(const char *text)
{
    pet_page_t *page = s_page;
    if (!page || !page->toast || !text) {
        return;
    }
    lv_label_set_text(page->toast, text);
    lv_obj_clear_flag(page->toast, LV_OBJ_FLAG_HIDDEN);
    if (page->toast_timer) {
        lv_timer_reset(page->toast_timer);
    } else {
        page->toast_timer = lv_timer_create(pet_toast_timer_cb, PET_TOAST_MS, page);
        lv_timer_set_repeat_count(page->toast_timer, 1);
    }
}

/* ---------------- event consumption ---------------- */

static void pet_consume_events(pet_page_t *page, uint32_t bits)
{
    if (!bits) {
        return;
    }

    static const struct {
        uint32_t evt;
        const char *toast;
        uint8_t tone; /* PET_TONE_NONE = no clip */
    } k_evt_map[] = {
        {PET_EVT_HATCH, "咔嚓……我出生啦！", PET_TONE_HATCH},
        {PET_EVT_EVOLVE, "哇！我长大了！", PET_TONE_EVOLVE},
        {PET_EVT_EAT, "啊呜啊呜~真好吃！", PET_TONE_NONE},
        {PET_EVT_CLEAN, "洗得香香软软的~", PET_TONE_NONE},
        {PET_EVT_PLAY, "耶！蹦蹦跳跳！", PET_TONE_NONE},
        {PET_EVT_MEDICINE, "药苦苦的…好多了！", PET_TONE_NONE},
        {PET_EVT_REJECT, "嗯……现在不太想", PET_TONE_NONE},
        {PET_EVT_SLEPT, "晚安，做个好梦~", PET_TONE_NONE},
        {PET_EVT_WOKE, "早上好！", PET_TONE_NONE},
        {PET_EVT_GREET, "你好呀！", PET_TONE_NONE},
        {PET_EVT_LOVE, "我也爱你！", PET_TONE_NONE},
        {PET_EVT_PAT, "好舒服呀~", PET_TONE_NONE},
        {PET_REMIND_HUNGRY, "我肚子饿得咕咕叫了，快喂我吃饭吧", PET_TONE_R_HUNGRY},
        {PET_REMIND_SAD, "好无聊呀，谁来陪我玩一会儿嘛", PET_TONE_R_SAD},
        {PET_REMIND_DIRTY, "我这里臭臭的，帮我打扫一下啦", PET_TONE_R_DIRTY},
        {PET_REMIND_TIRED, "我困了，说声关灯我就睡觉咯", PET_TONE_R_TIRED},
        {PET_REMIND_SICK, "呜…我好像生病了，给我吃点药吧", PET_TONE_R_SICK},
        {PET_REMIND_MISS, "好久没见到你了，我好想你呀", PET_TONE_R_MISS},
    };

    /* device-initiated speaking events wait for the cloud TTS to finish
     * instead of interrupting it; their bits stay pending and retry on the
     * next 1 s tick */
    const uint32_t k_speak_bits = PET_EVT_HATCH | PET_EVT_EVOLVE | PET_REMIND_HUNGRY |
                                  PET_REMIND_SAD | PET_REMIND_DIRTY | PET_REMIND_TIRED |
                                  PET_REMIND_SICK | PET_REMIND_MISS;
    if ((bits & k_speak_bits) && pet_voice_tts_active()) {
        bits &= ~k_speak_bits;
        if (!bits) {
            return;
        }
    }

    uint32_t processed = 0;
    for (size_t i = 0; i < sizeof(k_evt_map) / sizeof(k_evt_map[0]); i++) {
        if (bits & k_evt_map[i].evt) {
            if (k_evt_map[i].evt <= PET_EVT_PAT) {
                pet_draw_play_event(&page->draw_ctx, k_evt_map[i].evt);
                pet_buzzer_post(1200, 80); /* short blip, voice line came separately */
            } else {
                /* device-initiated lines (reminders/hatch/evolve) always
                 * speak locally; action reactions are spoken by the caller
                 * (offline command path) or the cloud (online path) */
                pet_voice_play(k_evt_map[i].tone);
            }
            pet_ui_toast(k_evt_map[i].toast);
            LISA_LOGI(TAG, "evt 0x%x: %s", k_evt_map[i].evt, k_evt_map[i].toast);
            processed |= k_evt_map[i].evt;
        }
    }
    pet_core_evt_ack(processed);
}

/* ---------------- timers ---------------- */

static void pet_anim_timer_cb(lv_timer_t *timer)
{
    pet_page_t *page = timer ? timer->user_data : NULL;
    if (!page) {
        return;
    }
    page->draw_ctx.t_ms += PET_ANIM_PERIOD_MS;
    pet_draw_scene(page->canvas, &page->last_snap, &page->draw_ctx);
}

static void pet_sync_widgets(pet_page_t *page)
{
    pet_snapshot_t snap;
    static const char *const stage_names[] = {"蛋", "幼年", "成年"};

    if (!page) {
        return;
    }
    pet_core_snapshot(&snap);

    /* stats */
    uint8_t vals[4] = {snap.satiety, snap.happy, snap.clean, snap.energy};
    for (int i = 0; i < 4; i++) {
        if (vals[i] != page->stat_val[i]) {
            page->stat_val[i] = vals[i];
            lv_bar_set_value(page->stat_bar[i], vals[i], LV_ANIM_OFF);
        }
    }

    /* stage line */
    if (snap.stage != page->last_stage || snap.age_days != page->last_age_days) {
        page->last_stage = snap.stage;
        page->last_age_days = snap.age_days;
        snprintf(page->stage_text, sizeof(page->stage_text), "%s · 第 %u 天%s",
                 stage_names[snap.stage], snap.age_days + 1,
                 snap.sleeping ? " · 睡觉中" : "");
        lv_label_set_text(page->stage_label, page->stage_text);
    }

    page->last_snap = snap;
    pet_consume_events(page, snap.evt_bits);
}

static void pet_core_timer_cb(lv_timer_t *timer)
{
    pet_page_t *page = timer ? timer->user_data : NULL;
    if (!page) {
        return;
    }
    pet_core_tick();
    pet_sync_widgets(page);
}

/* ---------------- nav screen ---------------- */

static int pet_page_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    (void)scr;
    pet_page_t *page = lisa_mem_calloc(1, sizeof(*page));
    if (!page) {
        return -1;
    }

    page->canvas_buf = lisa_mem_calloc(1, LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(PET_CANVAS_W, PET_CANVAS_H));
    if (!page->canvas_buf) {
        lisa_mem_free(page);
        return -1;
    }

    page->root = lv_obj_create(lv_scr_act());
    if (!page->root) {
        goto fail;
    }
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(page->root, 0, 0);
    lv_obj_set_size(page->root, PET_SCREEN_W, PET_SCREEN_H);
    lv_obj_set_style_pad_all(page->root, 0, 0);
    lv_obj_set_style_border_width(page->root, 0, 0);
    lv_obj_set_style_radius(page->root, 0, 0);
    lv_obj_set_style_bg_color(page->root, lv_color_hex(0xFFF8EC), 0);
    lv_obj_set_style_bg_opa(page->root, LV_OPA_COVER, 0);

    /* status groups */
    for (int i = 0; i < 4; i++) {
        page->stat_label[i] = lv_label_create(page->root);
        if (!page->stat_label[i]) {
            goto fail;
        }
        lv_obj_set_style_text_font(page->stat_label[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(page->stat_label[i], lv_color_hex(0x6B5B45), 0);
        lv_label_set_text(page->stat_label[i], k_stat_names[i]);
        lv_obj_set_pos(page->stat_label[i], i * PET_STAT_GROUP_W + PET_STAT_BAR_W / 2 + 2, 2);

        page->stat_bar[i] = lv_bar_create(page->root);
        if (!page->stat_bar[i]) {
            goto fail;
        }
        lv_obj_set_size(page->stat_bar[i], PET_STAT_BAR_W, 7);
        lv_obj_set_pos(page->stat_bar[i], i * PET_STAT_GROUP_W + 4, 22);
        lv_bar_set_range(page->stat_bar[i], 0, 100);
        lv_bar_set_value(page->stat_bar[i], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(page->stat_bar[i], lv_color_hex(0xE8DFC8), 0);
        lv_obj_set_style_bg_opa(page->stat_bar[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(page->stat_bar[i],
                                  (lv_color_t){.full = k_stat_colors[i]},
                                  LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    /* stage line */
    page->stage_label = lv_label_create(page->root);
    if (!page->stage_label) {
        goto fail;
    }
    lv_obj_set_style_text_font(page->stage_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(page->stage_label, lv_color_hex(0x8A7B5C), 0);
    lv_label_set_text(page->stage_label, "蛋 · 第 1 天");
    lv_obj_set_pos(page->stage_label, 0, 32);
    lv_obj_set_width(page->stage_label, PET_SCREEN_W);
    lv_obj_set_style_text_align(page->stage_label, LV_TEXT_ALIGN_CENTER, 0);

    /* character canvas */
    page->canvas = lv_canvas_create(page->root);
    if (!page->canvas) {
        goto fail;
    }
    lv_canvas_set_buffer(page->canvas, page->canvas_buf, PET_CANVAS_W, PET_CANVAS_H,
                         LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_pos(page->canvas, (PET_SCREEN_W - PET_CANVAS_W) / 2, 44);
    pet_draw_ctx_reset(&page->draw_ctx);

    /* action hint row */
    static const char *const hints[PET_HINT_NUM] = {"喂食", "清洁", "玩耍", "睡觉", "吃药", "状态"};
    for (int i = 0; i < PET_HINT_NUM; i++) {
        page->hint[i] = lv_label_create(page->root);
        if (!page->hint[i]) {
            goto fail;
        }
        lv_obj_set_style_text_font(page->hint[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(page->hint[i], lv_color_hex(0x9A8A6A), 0);
        lv_label_set_text(page->hint[i], hints[i]);
        lv_obj_set_pos(page->hint[i], 12 + i * 38, 214);
    }
    page->hint_sel = -1;

    /* toast */
    page->toast = lv_label_create(page->root);
    if (!page->toast) {
        goto fail;
    }
    lv_obj_set_style_text_font(page->toast, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(page->toast, lv_color_hex(0x4A3B28), 0);
    lv_obj_set_style_bg_color(page->toast, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(page->toast, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(page->toast, 6, 0);
    lv_obj_set_width(page->toast, PET_SCREEN_W - 40);
    lv_label_set_long_mode(page->toast, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(page->toast, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(page->toast, 20, 96);
    lv_obj_add_flag(page->toast, LV_OBJ_FLAG_HIDDEN);

    /* first snapshot so the first frame has data */
    pet_core_snapshot(&page->last_snap);
    page->last_stage = (pet_stage_t)-1;
    page->last_age_days = 0xFFFF;

    page->anim_timer = lv_timer_create(pet_anim_timer_cb, PET_ANIM_PERIOD_MS, page);
    page->core_timer = lv_timer_create(pet_core_timer_cb, PET_CORE_PERIOD_MS, page);
    if (!page->anim_timer || !page->core_timer) {
        goto fail;
    }

    s_page = page;
    lv_timer_ready(page->core_timer);
    pet_core_timer_cb(page->core_timer); /* paint bars/stage immediately */
    model_voice_on();
    *data = page;
    return 0;

fail:
    if (page->anim_timer) {
        lv_timer_del(page->anim_timer);
    }
    if (page->core_timer) {
        lv_timer_del(page->core_timer);
    }
    if (page->root) {
        lv_obj_del(page->root);
    }
    lisa_mem_free(page->canvas_buf);
    lisa_mem_free(page);
    return -1;
}

static int pet_page_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    pet_page_t *page = data;
    s_page = NULL;
    if (page) {
        if (page->toast_timer) {
            lv_timer_del(page->toast_timer);
        }
        if (page->anim_timer) {
            lv_timer_del(page->anim_timer);
        }
        if (page->core_timer) {
            lv_timer_del(page->core_timer);
        }
        if (page->root) {
            lv_obj_del(page->root);
        }
        lisa_mem_free(page->canvas_buf);
        lisa_mem_free(page);
    }
    return 0;
}

static int pet_page_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    pet_page_t *page = data;
    lv_timer_pause(page->anim_timer);
    lv_timer_pause(page->core_timer);
    lv_obj_add_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int pet_page_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    pet_page_t *page = data;
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    lv_timer_resume(page->anim_timer);
    lv_timer_resume(page->core_timer);
    lv_timer_ready(page->core_timer);
    model_voice_on();
    return 0;
}

static const struct lisa_ui_nav_scr s_pet_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_PET,
    .open = pet_page_open,
    .close = pet_page_close,
    .pause = pet_page_pause,
    .resume = pet_page_resume,
};

int pet_ui_init(void)
{
    return lisa_ui_nav_scr_add(&s_pet_nav_scr);
}

static void pet_ui_default_worker(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;
    (void)lisa_ui_nav_scr_default_set(&s_pet_nav_scr);
}

int pet_ui_become_default(void)
{
    return LISA_UI_INVOKE_UI(pet_ui_default_worker, NULL, 0);
}

/* Immediate one-shot widget refresh (called after shell/debug mutations so
 * the screen reflects them without waiting for the 1 s timer). */
static void pet_ui_kick_worker(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;
    pet_sync_widgets(s_page);
}

void pet_ui_kick(void)
{
    (void)LISA_UI_INVOKE_UI(pet_ui_kick_worker, NULL, 0);
}
