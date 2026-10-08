#define LOG_TAG "sip_call_view"

#include <stdio.h>
#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_fonts.h"
#include "sip_call_view.h"

/*
 * SIP 通话页视图：单页多态。远端视频用 LV_IMG_CF_TRUE_COLOR 换指针显示
 * （与 video_player_view 同一手法，零拷贝）；状态/对端/提示三行文字 +
 * 通话计时（ACTIVE 态 1s lv_timer 刷新）。
 * 所有入口都在 UI 线程被调用（模型层负责投递 worq.ui）。
 */

struct sip_view_ctx {
    lv_obj_t *root;
    lv_obj_t *img;
    lv_img_dsc_t dsc;
    int shown_w, shown_h;
    lv_obj_t *status_label;
    lv_obj_t *peer_label;
    lv_obj_t *hint_label;
    lv_timer_t *timer;
    int state;
    uint32_t start_tick;
};

static struct sip_view_ctx s_view;

static void sip_view_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_view.state != SIP_UI_ACTIVE || !s_view.status_label) {
        return;
    }
    uint32_t elapsed = (lv_tick_get() - s_view.start_tick) / 1000u;
    char text[32];
    snprintf(text, sizeof(text), "%02u:%02u", elapsed / 60u, elapsed % 60u);
    lv_label_set_text(s_view.status_label, text);
}

lv_obj_t *lisa_ui_sip_view_create(lv_obj_t *parent)
{
    memset(&s_view, 0, sizeof(s_view));

    s_view.root = lv_obj_create(parent);
    if (!s_view.root) {
        return NULL;
    }
    lv_obj_set_size(s_view.root, lv_pct(100), lv_pct(100));
    lv_obj_center(s_view.root);
    lv_obj_set_style_bg_color(s_view.root, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_view.root, 0, 0);
    lv_obj_set_style_radius(s_view.root, 0, 0);
    lv_obj_clear_flag(s_view.root, LV_OBJ_FLAG_SCROLLABLE);

    /* 远端视频：初始 1x1 占位，首帧到达后填真实尺寸。 */
    s_view.img = lv_img_create(s_view.root);
    if (!s_view.img) {
        lv_obj_del(s_view.root);
        memset(&s_view, 0, sizeof(s_view));
        return NULL;
    }
    lv_obj_center(s_view.img);
    lv_img_set_antialias(s_view.img, false);
    static uint16_t blank = 0;
    s_view.dsc.header.always_zero = 0;
    s_view.dsc.header.w = 1;
    s_view.dsc.header.h = 1;
    s_view.dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_view.dsc.data = (const uint8_t *)&blank;
    s_view.dsc.data_size = sizeof(blank);
    lv_img_set_src(s_view.img, &s_view.dsc);

    s_view.status_label = lv_label_create(s_view.root);
    s_view.peer_label = lv_label_create(s_view.root);
    s_view.hint_label = lv_label_create(s_view.root);
    if (!s_view.status_label || !s_view.peer_label || !s_view.hint_label) {
        lv_obj_del(s_view.root);
        memset(&s_view, 0, sizeof(s_view));
        return NULL;
    }
    lv_obj_t *labels[] = {s_view.status_label, s_view.peer_label, s_view.hint_label};
    for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        lv_obj_set_style_text_font(labels[i], &lv_font_chinese_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(labels[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_width(labels[i], lv_pct(100));
        lv_label_set_long_mode(labels[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(labels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_add_flag(labels[i], LV_OBJ_FLAG_IGNORE_LAYOUT);
    }
    lv_obj_align(s_view.status_label, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_align(s_view.peer_label, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_align(s_view.hint_label, LV_ALIGN_BOTTOM_MID, 0, -16);

    s_view.timer = lv_timer_create(sip_view_timer_cb, 1000, NULL);
    if (s_view.timer) {
        lv_timer_pause(s_view.timer);
    }

    lisa_ui_sip_view_set_state(SIP_UI_DIAL, NULL);
    return s_view.root;
}

void lisa_ui_sip_view_set_state(int ui_state, const char *peer)
{
    if (!s_view.root) {
        return;
    }
    s_view.state = ui_state;

    char peer_text[48];
    if (peer && peer[0]) {
        snprintf(peer_text, sizeof(peer_text), "对端 %s", peer);
    } else {
        snprintf(peer_text, sizeof(peer_text), "对端未配置（sip cfg <ip>）");
    }
    lv_label_set_text(s_view.peer_label, peer_text);

    switch (ui_state) {
    case SIP_UI_DIAL:
        lv_obj_add_flag(s_view.img, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_view.status_label, "视频通话");
        lv_label_set_text(s_view.hint_label, "单击 呼叫   长按 返回");
        if (s_view.timer) {
            lv_timer_pause(s_view.timer);
        }
        break;
    case SIP_UI_OUTGOING:
        lv_obj_add_flag(s_view.img, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_view.status_label, "正在呼叫...");
        lv_label_set_text(s_view.hint_label, "单击 取消");
        if (s_view.timer) {
            lv_timer_pause(s_view.timer);
        }
        break;
    case SIP_UI_INCOMING:
        lv_obj_add_flag(s_view.img, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_view.status_label, "来电");
        lv_label_set_text(s_view.hint_label, "单击 接听   长按 拒接");
        if (s_view.timer) {
            lv_timer_pause(s_view.timer);
        }
        break;
    case SIP_UI_ACTIVE:
        lv_obj_clear_flag(s_view.img, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_view.hint_label, "单击 挂断");
        s_view.start_tick = lv_tick_get();
        if (s_view.timer) {
            lv_timer_reset(s_view.timer);
            lv_timer_resume(s_view.timer);
        }
        sip_view_timer_cb(NULL);
        break;
    case SIP_UI_ENDED:
    default:
        lv_obj_add_flag(s_view.img, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_view.status_label, "已结束");
        lv_label_set_text(s_view.hint_label, "");
        if (s_view.timer) {
            lv_timer_pause(s_view.timer);
        }
        break;
    }
}

void lisa_ui_sip_view_set_frame(const uint16_t *rgb565, int w, int h)
{
    if (!s_view.img || !rgb565 || w <= 0 || h <= 0) {
        return;
    }

    int size_changed = (w != s_view.shown_w || h != s_view.shown_h);
    s_view.shown_w = w;
    s_view.shown_h = h;
    s_view.dsc.header.w = (uint32_t)w;
    s_view.dsc.header.h = (uint32_t)h;
    s_view.dsc.data = (const uint8_t *)rgb565;
    s_view.dsc.data_size = (uint32_t)w * h * sizeof(uint16_t);

    if (size_changed) {
        lv_img_set_src(s_view.img, &s_view.dsc);
        lv_obj_center(s_view.img);
    }
    lv_obj_invalidate(s_view.img);
}
