#define LOG_TAG "video_player_view"

#include <string.h>

#include "lisa_ui.h"
#include "video_player_view.h"

/*
 * 视频页视图：全屏黑底 + 一张 LV_IMG_CF_TRUE_COLOR 图像。帧数据不拷贝，
 * 播放服务在双缓冲间切换后调用 set_frame 更新指针并失效重绘。
 * LV_COLOR_DEPTH=16 时 TRUE_COLOR 即 RGB565，与服务侧转换输出一致。
 */

struct video_view_ctx {
    lv_obj_t *root;
    lv_obj_t *img;
    lv_img_dsc_t dsc;
    int shown_w, shown_h;
};

static struct video_view_ctx s_view;

lv_obj_t *lisa_ui_video_view_create(lv_obj_t *parent) {
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

    s_view.img = lv_img_create(s_view.root);
    if (!s_view.img) {
        lv_obj_del(s_view.root);
        memset(&s_view, 0, sizeof(s_view));
        return NULL;
    }
    lv_obj_center(s_view.img);
    lv_img_set_antialias(s_view.img, false);
    /* 初始 1x1 透明占位，首帧到达后由 set_frame 填入真实尺寸 */
    static uint16_t blank = 0;
    s_view.dsc.header.always_zero = 0;
    s_view.dsc.header.w = 1;
    s_view.dsc.header.h = 1;
    s_view.dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_view.dsc.data = (const uint8_t *)&blank;
    s_view.dsc.data_size = sizeof(blank);
    lv_img_set_src(s_view.img, &s_view.dsc);

    return s_view.root;
}

void lisa_ui_video_view_set_frame(const uint16_t *rgb565, int w, int h) {
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
        /* 尺寸变化需重新走 set_src 更新内部缓存，再居中 */
        lv_img_set_src(s_view.img, &s_view.dsc);
        lv_obj_center(s_view.img);
    }
    lv_obj_invalidate(s_view.img);
}
