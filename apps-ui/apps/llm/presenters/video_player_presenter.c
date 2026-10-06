#define LOG_TAG "video_player_presenter"

#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "model_video_player.h"
#include "video_player_view.h"

/*
 * 视频播放页：open 时创建视图并订阅播放状态，close 时释放。
 * 播放本身由固件侧 video_player 服务驱动，页面只负责显示
 * （服务每帧通过 lisa_ui_video_view_set_frame 推送）。
 */

struct video_nav_scr_data {
    lv_obj_t *view;
    int playing_at_open;
};

static int video_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data) {
    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    struct video_nav_scr_data *scr_data =
        lisa_ui_malloc(sizeof(struct video_nav_scr_data));
    if (!scr_data) {
        return -1;
    }
    memset(scr_data, 0, sizeof(*scr_data));

    scr_data->view = lisa_ui_video_view_create(lv_scr_act());
    if (!scr_data->view) {
        lisa_ui_free(scr_data);
        return -1;
    }
    scr_data->playing_at_open = model_video_player_is_playing();

    *data = scr_data;
    return 0;
}

static int video_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data) {
    struct video_nav_scr_data *scr_data = data;

    LISA_UI_LOGD("nav scr show, id: %d", scr->unique_id);
    if (!scr_data || !scr_data->view) {
        return -1;
    }
    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int video_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data) {
    struct video_nav_scr_data *scr_data = data;

    LISA_UI_LOGD("nav scr pause, id: %d", scr->unique_id);
    (void)scr_data;
    /* 离开页面即停播（视频是实时流，后台继续播只浪费带宽） */
    model_video_player_stop();
    return 0;
}

static int video_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data) {
    (void)scr;
    (void)data;
    return 0;
}

static int video_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data) {
    struct video_nav_scr_data *scr_data = data;

    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);
    if (!scr_data) {
        return 0;
    }
    if (scr_data->view) {
        lv_obj_del(scr_data->view);
    }
    lisa_ui_free(scr_data);
    return 0;
}

const struct lisa_ui_nav_scr video_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_VIDEO,
    .open = video_nav_scr_open,
    .show = video_nav_scr_show,
    .pause = video_nav_scr_pause,
    .resume = video_nav_scr_resume,
    .close = video_nav_scr_close,
};
