/*
 * ss_panel.c - SoundSense detection panel (default screen)
 *
 * Takes over the home page: shows per-class last event time and total count
 * plus a live probability bar, refreshed every second from ss_core.
 * Layout (240x240):
 *   row 0   title + connection state dot
 *   row 1   snoring block: name / count / last event / prob bar
 *   row 2   baby_cry block: same
 *   row 3   footer: server url (truncated) / uptime
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "lisa_log.h"
#include "lisa_ui_fonts.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lvgl.h"
#include "models/model_voice.h"

#include "ss_core.h"
#include "ss_panel.h"

#define TAG "ss"

typedef struct {
    lv_obj_t *root;
    lv_timer_t *timer;

    lv_obj_t *title;
    lv_obj_t *conn_state;

    /* per-class widgets */
    lv_obj_t *class_label[SS_CLASS_NUM];   /* "😴 鼾声" / "👶 哭声" */
    lv_obj_t *class_count[SS_CLASS_NUM];    /* "共 N 次" */
    lv_obj_t *class_last[SS_CLASS_NUM];     /* "最近: HH:MM (12m ago)" */
    lv_obj_t *class_bar[SS_CLASS_NUM];      /* probability bar */

    lv_obj_t *footer;

    /* cached counters */
    uint32_t class_counts[SS_CLASS_NUM];
    uint32_t last_event_ts[SS_CLASS_NUM];
    bool last_was_start[SS_CLASS_NUM];
} ss_panel_t;

static ss_panel_t *s_panel;

static const char *const k_class_display[SS_CLASS_NUM] = {"😴 鼾声", "👶 哭声"};
static const lv_color_t k_class_color[SS_CLASS_NUM] = {
    /* teal / red in RGB565 */
    {.full = 0x07E0},  /* snoring: green */
    {.full = 0xF800},  /* baby_cry: red */
};

/* ------------------------------------------------------------------ */

static void ss_panel_refresh(void)
{
    if (!s_panel) return;

    ss_status_t st;
    ss_core_get_status(&st);

    /* connection indicator */
    const char *conn;
    switch (st.state) {
    case SS_STATE_STREAMING:  conn = "#00ff00 ●"; break;
    case SS_STATE_CONNECTING:  conn = "#ffff00 ●"; break;
    case SS_STATE_RETRY_WAIT:  conn = "#ff8800 ●"; break;
    default:                   conn = "#888888 ●"; break;
    }
    lv_label_set_text(s_panel->conn_state, conn);

    /* probability bars */
    for (int i = 0; i < SS_CLASS_NUM; i++) {
        int32_t prom = st.last_probs_x1000[i];
        if (prom < 0) prom = 0;
        uint32_t pct = (uint32_t)prom / 10; /* x1000 → percent */
        if (pct > 100) pct = 100;
        lv_bar_set_value(s_panel->class_bar[i], pct, LV_ANIM_OFF);
    }

    /* per-class info from running totals (no ring iteration needed) */
    for (int c = 0; c < SS_CLASS_NUM; c++) {
        char buf[48];
        snprintf(buf, sizeof(buf), "共 %u 次", st.class_event_count[c]);
        lv_label_set_text(s_panel->class_count[c], buf);

        if (st.class_last_ts_ms[c] > 0) {
            time_t sec = st.class_last_ts_ms[c] / 1000 + 8 * 3600;
            struct tm tm_s;
            gmtime_r(&sec, &tm_s);
            snprintf(buf, sizeof(buf), "最近: %02d:%02d%s",
                     tm_s.tm_hour, tm_s.tm_min,
                     st.class_last_is_start[c] ? "" : " 止");
        } else {
            snprintf(buf, sizeof(buf), "暂无事件");
        }
        lv_label_set_text(s_panel->class_last[c], buf);
    }

    /* footer */
    char footer[64];
    snprintf(footer, sizeof(footer), "up %us | %u/%u", st.uptime_s,
             st.send_frames, st.send_drops);
    lv_label_set_text(s_panel->footer, footer);
}

static void ss_panel_timer_cb(lv_timer_t *t)
{
    (void)t;
    ss_panel_refresh();
}

/* ------------------------------------------------------------------ */
/* nav screen callbacks                                                */
/* ------------------------------------------------------------------ */

static int ss_panel_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    (void)scr;
    ss_panel_t *p = lv_mem_alloc(sizeof(*p));
    if (!p) return -1;
    memset(p, 0, sizeof(*p));

    p->root = lv_obj_create(lv_scr_act());
    if (!p->root) { lv_mem_free(p); return -1; }
    lv_obj_clear_flag(p->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(p->root, 0, 0);
    lv_obj_set_size(p->root, 240, 240);
    lv_obj_set_style_pad_all(p->root, 8, 0);
    lv_obj_set_style_border_width(p->root, 0, 0);
    lv_obj_set_style_radius(p->root, 0, 0);
    lv_obj_set_style_bg_color(p->root, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_opa(p->root, LV_OPA_COVER, 0);

    /* header */
    p->title = lv_label_create(p->root);
    lv_label_set_text(p->title, "🔊 声音监测");
    lv_obj_set_style_text_font(p->title, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(p->title, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_pos(p->title, 0, 2);

    p->conn_state = lv_label_create(p->root);
    lv_label_set_recolor(p->conn_state, true);
    lv_obj_set_pos(p->conn_state, 180, 4);

    /* class blocks */
    for (int i = 0; i < SS_CLASS_NUM; i++) {
        int y_base = 34 + i * 78;

        p->class_label[i] = lv_label_create(p->root);
        lv_label_set_text(p->class_label[i], k_class_display[i]);
        lv_obj_set_style_text_font(p->class_label[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(p->class_label[i], k_class_color[i], 0);
        lv_obj_set_pos(p->class_label[i], 0, y_base);

        p->class_count[i] = lv_label_create(p->root);
        lv_obj_set_style_text_font(p->class_count[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(p->class_count[i], lv_color_hex(0xc0c0c0), 0);
        lv_obj_set_pos(p->class_count[i], 100, y_base);

        p->class_last[i] = lv_label_create(p->root);
        lv_obj_set_style_text_font(p->class_last[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(p->class_last[i], lv_color_hex(0x888888), 0);
        lv_obj_set_pos(p->class_last[i], 0, y_base + 22);

        p->class_bar[i] = lv_bar_create(p->root);
        lv_obj_set_size(p->class_bar[i], 220, 8);
        lv_obj_set_pos(p->class_bar[i], 0, y_base + 46);
        lv_bar_set_range(p->class_bar[i], 0, 100);
        lv_bar_set_value(p->class_bar[i], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(p->class_bar[i], lv_color_hex(0x30363d), 0);
        lv_obj_set_style_bg_opa(p->class_bar[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(p->class_bar[i], k_class_color[i], LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(p->class_bar[i], LV_OPA_COVER, LV_PART_INDICATOR);
    }

    /* footer */
    p->footer = lv_label_create(p->root);
    lv_obj_set_style_text_font(p->footer, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(p->footer, lv_color_hex(0x484f58), 0);
    lv_obj_set_pos(p->footer, 0, 210);

    p->timer = lv_timer_create(ss_panel_timer_cb, 1000, p);
    if (!p->timer) {
        lv_obj_del(p->root);
        lv_mem_free(p);
        return -1;
    }

    s_panel = p;
    ss_panel_refresh();
    model_voice_on();
    *data = p;
    return 0;
}

static int ss_panel_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    ss_panel_t *p = data;
    s_panel = NULL;
    if (p) {
        if (p->timer) lv_timer_del(p->timer);
        if (p->root) lv_obj_del(p->root);
        lv_mem_free(p);
    }
    return 0;
}

static int ss_panel_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    ss_panel_t *p = data;
    if (p) {
        lv_timer_pause(p->timer);
        lv_obj_add_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    }
    return 0;
}

static int ss_panel_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    ss_panel_t *p = data;
    if (p) {
        lv_obj_clear_flag(p->root, LV_OBJ_FLAG_HIDDEN);
        lv_timer_resume(p->timer);
        lv_timer_ready(p->timer);
        ss_panel_refresh();
    }
    return 0;
}

static const struct lisa_ui_nav_scr s_ss_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SS,
    .open = ss_panel_open,
    .close = ss_panel_close,
    .pause = ss_panel_pause,
    .resume = ss_panel_resume,
};

int ss_panel_init(void)
{
    int ret = lisa_ui_nav_scr_add(&s_ss_nav_scr);
    if (ret != 0) return ret;
    /* take over the default screen from home */
    lisa_ui_nav_scr_default_set(&s_ss_nav_scr);
    LISA_LOGI(TAG, "panel: registered as default screen");
    return 0;
}
