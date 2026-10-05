/*
 * ss_panel.c - SoundSense detection panel (default screen)
 *
 * Takes over the home page: shows per-class last event time and total count
 * plus a live probability bar, refreshed every second from ss_core.
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

#include "ss_core.h"
#include "ss_panel.h"

#define TAG "ss"

typedef struct {
    lv_obj_t *root;
    lv_timer_t *timer;
    lv_obj_t *title;
    lv_obj_t *conn_state;
    lv_obj_t *class_label[SS_CLASS_NUM];
    lv_obj_t *class_count[SS_CLASS_NUM];
    lv_obj_t *class_last[SS_CLASS_NUM];
    lv_obj_t *class_bar[SS_CLASS_NUM];
    lv_obj_t *footer;
} ss_panel_t;

static ss_panel_t *s_panel;

static const char *const k_class_display[SS_CLASS_NUM] = {"鼾声", "哭声"};
static const uint16_t k_class_color565[SS_CLASS_NUM] = {0x07E0, 0xF800};

static void ss_panel_refresh(void)
{
    if (!s_panel || !s_panel->root) return;

    ss_status_t st;
    ss_core_get_status(&st);

    const char *conn;
    uint32_t conn_color;
    switch (st.state) {
    case SS_STATE_STREAMING:  conn = "在线";  conn_color = 0x2ea043; break;
    case SS_STATE_CONNECTING:  conn = "连接中"; conn_color = 0xd29922; break;
    case SS_STATE_RETRY_WAIT:  conn = "重试";  conn_color = 0xd29922; break;
    default:                   conn = "离线";  conn_color = 0x6e7681; break;
    }
    lv_label_set_text(s_panel->conn_state, conn);
    lv_obj_set_style_text_color(s_panel->conn_state, lv_color_hex(conn_color), 0);

    for (int i = 0; i < SS_CLASS_NUM; i++) {
        int32_t prom = st.last_probs_x1000[i];
        if (prom < 0) prom = 0;
        uint32_t pct = (uint32_t)prom / 10;
        if (pct > 100) pct = 100;
        lv_bar_set_value(s_panel->class_bar[i], pct, LV_ANIM_OFF);

        char buf[48];
        snprintf(buf, sizeof(buf), "%s: %u 次", k_class_display[i],
                 st.class_event_count[i]);
        lv_label_set_text(s_panel->class_label[i], buf);

        if (st.class_last_time[i][0] != '\0') {
            snprintf(buf, sizeof(buf), "最近 %s%s", st.class_last_time[i],
                     st.class_last_is_start[i] ? "" : " 结束");
        } else {
            snprintf(buf, sizeof(buf), "暂无事件");
        }
        lv_label_set_text(s_panel->class_last[i], buf);
    }

    char footer[48];
    snprintf(footer, sizeof(footer), "运行 %us  丢帧 %u", st.uptime_s,
             st.send_drops);
    lv_label_set_text(s_panel->footer, footer);
}

static void ss_panel_timer_cb(lv_timer_t *t)
{
    (void)t;
    ss_panel_refresh();
}

static int ss_panel_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    (void)scr;
    ss_panel_t *p = lisa_mem_calloc(1, sizeof(*p));
    if (!p) return -1;

    p->root = lv_obj_create(lv_scr_act());
    if (!p->root) { lisa_mem_free(p); return -1; }
    lv_obj_clear_flag(p->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(p->root, 0, 0);
    lv_obj_set_size(p->root, 240, 240);
    lv_obj_set_style_pad_all(p->root, 8, 0);
    lv_obj_set_style_border_width(p->root, 0, 0);
    lv_obj_set_style_radius(p->root, 0, 0);
    lv_obj_set_style_bg_color(p->root, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_opa(p->root, LV_OPA_COVER, 0);

    p->title = lv_label_create(p->root);
    if (!p->title) goto fail;
    lv_obj_set_style_text_font(p->title, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(p->title, lv_color_hex(0xe0e0e0), 0);
    lv_label_set_text(p->title, "声音监测");
    lv_obj_set_pos(p->title, 0, 2);

    p->conn_state = lv_label_create(p->root);
    if (!p->conn_state) goto fail;
    lv_obj_set_style_text_font(p->conn_state, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(p->conn_state, lv_color_hex(0x00ff00), 0);
    lv_label_set_text(p->conn_state, "离线");
    lv_obj_set_pos(p->conn_state, 176, 2);

    for (int i = 0; i < SS_CLASS_NUM; i++) {
        int y = 36 + i * 76;

        p->class_label[i] = lv_label_create(p->root);
        if (!p->class_label[i]) goto fail;
        lv_obj_set_style_text_font(p->class_label[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(p->class_label[i],
            lv_color_hex(i == 0 ? 0x4ecdc4 : 0xff6b6b), 0);
        lv_label_set_text(p->class_label[i], k_class_display[i]);
        lv_obj_set_pos(p->class_label[i], 0, y);

        p->class_last[i] = lv_label_create(p->root);
        if (!p->class_last[i]) goto fail;
        lv_obj_set_style_text_font(p->class_last[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_color(p->class_last[i], lv_color_hex(0x888888), 0);
        lv_label_set_text(p->class_last[i], "暂无事件");
        lv_obj_set_pos(p->class_last[i], 0, y + 22);

        p->class_bar[i] = lv_bar_create(p->root);
        if (!p->class_bar[i]) goto fail;
        lv_obj_set_size(p->class_bar[i], 220, 8);
        lv_obj_set_pos(p->class_bar[i], 0, y + 46);
        lv_bar_set_range(p->class_bar[i], 0, 100);
        lv_bar_set_value(p->class_bar[i], 0, LV_ANIM_OFF);
    }

    p->footer = lv_label_create(p->root);
    if (!p->footer) goto fail;
    lv_obj_set_style_text_font(p->footer, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(p->footer, lv_color_hex(0x484f58), 0);
    lv_label_set_text(p->footer, "");
    lv_obj_set_pos(p->footer, 0, 210);

    /* create timer AFTER all widgets are ready */
    p->timer = lv_timer_create(ss_panel_timer_cb, 1000, p);
    if (!p->timer) goto fail;

    s_panel = p;
    ss_panel_refresh();
    model_voice_on();
    *data = p;
    return 0;

fail:
    if (p->root) lv_obj_del(p->root);
    lisa_mem_free(p);
    return -1;
}

static int ss_panel_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    ss_panel_t *p = data;
    s_panel = NULL;
    if (p) {
        if (p->timer) lv_timer_del(p->timer);
        if (p->root) lv_obj_del(p->root);
        lisa_mem_free(p);
    }
    return 0;
}

static int ss_panel_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    ss_panel_t *p = data;
    if (p && p->timer) lv_timer_pause(p->timer);
    if (p && p->root) lv_obj_add_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int ss_panel_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    ss_panel_t *p = data;
    if (p && p->root) lv_obj_clear_flag(p->root, LV_OBJ_FLAG_HIDDEN);
    if (p && p->timer) {
        lv_timer_resume(p->timer);
        lv_timer_ready(p->timer);
    }
    if (p) ss_panel_refresh();
    if (p) model_voice_on();
    return 0;
}

static const struct lisa_ui_nav_scr s_ss_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_SS,
    .open = ss_panel_open,
    .close = ss_panel_close,
    .pause = ss_panel_pause,
    .resume = ss_panel_resume,
};

/* default_set 会关闭 home 并新建本页的整套 LVGL 对象；ss_panel_init 在
 * lisa_ui_lvgl_run 之后运行于 main 线程，必须投递到 worq.ui 执行，
 * 否则与 lv_task_handler 刷新循环并发操作对象树会踩坏样式链。 */
static void ss_panel_default_worker(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;
    if (lisa_ui_nav_scr_default_set(&s_ss_nav_scr) != 0) {
        LISA_LOGW(TAG, "panel: default_set failed, home stays default");
    }
}

int ss_panel_init(void)
{
    int ret = lisa_ui_nav_scr_add(&s_ss_nav_scr);
    if (ret != 0) return ret;
    if (lisa_ui_invoke_ui_delayed(ss_panel_default_worker, NULL, 0, 0) != 0) {
        LISA_LOGW(TAG, "panel: failed to queue default_set, home stays default");
        return 0;
    }
    LISA_LOGI(TAG, "panel: taking over as default screen");
    return 0;
}
