#include <stdint.h>

#include "lvgl.h"
#include "ebus/ebus.h"

#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "launcher_pages.h"
#include "lisa_ui_scr_ai_human_commu.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_assets.h"
#include "user_groups.h"
#include "lisaui_log.h"
#include "lisaui_user_data.h"

#include "../group_launcher.h"
#include "private/anim_images.h"

#define TAG "launcher.page.commu"

typedef struct {
    lv_obj_t *inter;
    ebus_chn_t *ebus_ch_base_event;

} page_view_t;

// 定义映射数组
static const char *remote_inter_state_map[] = {
    [LISAUI_USERDATA_INTER_REMOTE_STATE_IDLE] = "我在听...",
    [LISAUI_USERDATA_INTER_REMOTE_STATE_LISTENING] = "我在听...",
    [LISAUI_USERDATA_INTER_REMOTE_STATE_THINKING] = "思考中...",
    [LISAUI_USERDATA_INTER_REMOTE_STATE_TALKING] = "说话中...",
    // 其他状态可以根据需要映射
};


typedef struct{

    uint32_t duration;
    uint32_t images_count;
    void *images;    

} role_emoji_anim_t;

static const role_emoji_anim_t role_emoji_anim[ROLE_EMOJI_MAX + 1] = {
    [ROLE_EMOJI_UNKNOW] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_blink) / sizeof(emoji_blink[0]),
            .images = emoji_blink,
        },
    [ROLE_EMOJI_LOVE] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_heart) / sizeof(emoji_heart[0]),
            .images = emoji_heart,
        },
    [ROLE_EMOJI_SAD] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_sad) / sizeof(emoji_sad[0]),
            .images = emoji_sad,
        },
    [ROLE_EMOJI_LAUGH] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_mock) / sizeof(emoji_mock[0]),
            .images = emoji_mock,
        },
    [ROLE_EMOJI_ANGRY] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_angry) / sizeof(emoji_angry[0]),
            .images = emoji_angry,
        },
    [ROLE_EMOJI_EYE] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_squint) / sizeof(emoji_squint[0]),
            .images = emoji_squint,
        },
    [ROLE_EMOJI_BLINK] =
        {
            .duration = 1000,
            .images_count = sizeof(emoji_blink) / sizeof(emoji_blink[0]),
            .images = emoji_blink,
        },
};

static int update_inter_state(page_view_t *view)
{

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {

        if (_userdata->setting.mic_is_mute) {
            lisa_ui_scr_ai_human_commu_talk_txt_img_set(view->inter, "已关麦...");
            lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "请打开麦克风跟我说话");
        } else {
            lisa_ui_scr_ai_human_commu_talk_txt_img_set(view->inter, remote_inter_state_map[_userdata->inter.remote_state]);
            switch (_userdata->inter.local_state) {
            case LISAUI_USERDATA_INTER_LOCAL_STATE_IDLE:
                lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "请唤醒我");
                lisa_ui_scr_ai_human_commu_label_me_set_txt(view->inter, "请唤醒我");
                break;
            case LISAUI_USERDATA_INTER_LOCAL_STATE_WAKEUP:
                lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "请说话...");
                break;
            case LISAUI_USERDATA_INTER_LOCAL_STATE_RECOGNITION:
                if (!(_userdata->roles.roles[_userdata->roles.role_idx].is_emoji)) {
                    if (_userdata->inter.iat_text != NULL) {
                        lisa_ui_scr_ai_human_commu_talk_anim_start(view->inter);
                        lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, _userdata->inter.iat_text);
                        lisa_ui_scr_ai_human_commu_label_me_set_txt(view->inter, _userdata->inter.iat_text);
                    } else {
                        lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "");
                    }
                }
                break;
            default:
                break;
            }
        }


        LISAUI_LOGI(TAG, "lisaui update inter state,remote:%d,local:%d,iat_text:%s", _userdata->inter.remote_state,
                    _userdata->inter.local_state, _userdata->inter.iat_text);
    }
}

static int update_role_emoji(page_view_t *view)
{

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {

        if((_userdata->roles.role_idx == ANIM_ROLE_INDEX) 
            && (_userdata->roles.roles[_userdata->roles.role_idx].emoji < ROLE_EMOJI_MAX)){

            lisa_ui_scr_ai_human_commu_talk_emoji_anim_set(view->inter, 
                role_emoji_anim[_userdata->roles.roles[_userdata->roles.role_idx].emoji].images,
                role_emoji_anim[_userdata->roles.roles[_userdata->roles.role_idx].emoji].images_count);
    
            lisa_ui_scr_ai_human_commu_talk_emoji_anim_start(view->inter, 
                role_emoji_anim[_userdata->roles.roles[_userdata->roles.role_idx].emoji].duration);
        }

        LISAUI_LOGI(TAG,"[%s]role_idx:%d,emoji:%d", __FUNCTION__, _userdata->roles.role_idx, _userdata->roles.roles[_userdata->roles.role_idx].emoji);

    }
}

static int event_inter_state_handler(ebus_chn_t *chn, uint32_t code,void *message, uint32_t msg_size, void *user_data)
{
    page_view_t *view = (page_view_t *)user_data;

    if (view == NULL) {
        return -1;
    }

    switch (code) {

    case LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE:
        update_inter_state(view);
        break;
    case LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE:
        update_role_emoji(view);
        break;
    case LISAUI_EBUS_CH_EVENT_M2U_INTER_END: {
        bool is_emoji = false;
        bool is_mute = false;
        LISAUI_USERDATA_WITH_LOCK(_userdata)
        {
            if (_userdata->roles.roles[_userdata->roles.role_idx].is_emoji) {
                is_emoji = true;
            }
            _userdata->setting.mic_is_mute = true;
            is_mute = _userdata->setting.mic_is_mute;
        }
        LISAUI_LOGI(TAG, "event_inter_state_handler, is_mute:%d, is_emoji:%d", is_mute,
                    is_emoji);

        if (!is_emoji) {
            lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set((lv_obj_t *)view->inter, lv_color_hex(0xFF3A3A));
            lisa_ui_scr_ai_human_commu_btn_mute_img_set((lv_obj_t *)view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_mute));
            ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_MUTE_UPDATE, NULL, 0);
            lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "请打开麦克风跟我说话");
            lisa_ui_scr_ai_human_commu_talk_txt_img_set(view->inter, "已关麦...");
        } else {
            lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                       LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 0);
        }
    } break;
    default:

        break;
    }

    return 0;
}

static void btn_mute_click_event_cb(void *user_data)
{
    lisaui_page_t *page = (lisaui_page_t *)user_data;
    page_view_t *view = (page_view_t *)page->view;
    uint8_t muted;
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        _userdata->setting.mic_is_mute = !_userdata->setting.mic_is_mute;
        muted = _userdata->setting.mic_is_mute;
    }
    LISAUI_LOGI(TAG, "btn_mute_click_event_cb, muted:%d\n", muted);

    if (muted) {

        lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set((lv_obj_t *)view->inter, lv_color_hex(0xFF3A3A));
        lisa_ui_scr_ai_human_commu_btn_mute_img_set((lv_obj_t *)view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_mute));
        lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "请打开麦克风跟我说话");
        lisa_ui_scr_ai_human_commu_talk_txt_img_set(view->inter, "已关麦...");
    } else {

        lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set((lv_obj_t *)view->inter, lv_color_hex(0xFABE00));
        lisa_ui_scr_ai_human_commu_btn_mute_img_set((lv_obj_t *)view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_unmute));
        lisa_ui_scr_ai_human_commu_label_set_tips(view->inter, "请说话...");
    }

    ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_MUTE_UPDATE, NULL, 0);
}

static void btn_cam_click_event_cb(void *user_data)
{

    LISAUI_LOGI(TAG, "btn_cam_click_event_cb enter video page\n");
    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                               LISAUI_GROUP_LAUNCHER_PAGE_INDEX_VIDEO, LISAUI_PAGE_SAVE_TO_HISTORY);
}

static void update_role_image(page_view_t *view)
{
    char *name = NULL;
    void *img_commu = NULL;
    uint32_t index;
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        name = _userdata->roles.roles[_userdata->roles.role_idx].name;
        img_commu = _userdata->roles.roles[_userdata->roles.role_idx].img_commu;
        index = _userdata->roles.role_idx;
    }
    lisa_ui_scr_base_title_set(view->inter, name);

    if (ANIM_ROLE_INDEX == index) {
        lisa_ui_scr_ai_human_commu_talk_emoji_anim_set(view->inter, emoji_test, sizeof(emoji_test) / sizeof(emoji_test[0]));
        lisa_ui_scr_ai_human_commu_talk_emoji_anim_start(view->inter, ANIM_ROLE_DURATION);
    } else {
        lisa_ui_scr_ai_human_commu_img_set(view->inter, img_commu, name);
        lisa_ui_scr_ai_human_commu_talk_anim_set(view->inter, talking_anim, sizeof(talking_anim) / sizeof(talking_anim[0]));
    }
}

static lisaui_page_t *create(lisaui_page_t *page)
{
    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if (view == NULL) {
        LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
        goto _ERR;
    }



    view->inter = lisa_ui_scr_ai_human_commu_create(NULL, false);

    lisa_ui_scr_ai_human_commu_btn_mute_img_set(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_unmute));
    lisa_ui_scr_ai_human_commu_btn_cam_img_set(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_cam));
    lisa_ui_scr_ai_human_commu_talk_bg_img_set(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_talk_bg));
    lisa_ui_scr_ai_human_commu_talk_txt_img_set(view->inter, "我在听...");
    lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_set(view->inter, btn_mute_click_event_cb, page);
    lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_set(view->inter, btn_cam_click_event_cb, page);

    update_role_image(view);

    view->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (view->ebus_ch_base_event != NULL) {
        ebus_message_subscribe( view->ebus_ch_base_event, 
                                EBUS_SUBSCRIBER_TYPE_SYNC, 
                                LISAUI_EBUS_CH_EVENT_M2U_INTER_STATE_UPDATE,
                                event_inter_state_handler, 
                                view);

        ebus_message_subscribe( view->ebus_ch_base_event, 
                                EBUS_SUBSCRIBER_TYPE_SYNC, 
                                LISAUI_EBUS_CH_EVENT_M2U_ROLE_EMOJI_UPDATE,
                                event_inter_state_handler, 
                                view);
        ebus_message_subscribe( view->ebus_ch_base_event, 
                                EBUS_SUBSCRIBER_TYPE_SYNC, 
                                LISAUI_EBUS_CH_EVENT_M2U_INTER_END,
                                event_inter_state_handler, 
                                view);
    
        }

    page->view = view;

    LISAUI_LOGI(TAG, "Create %s page success:", page->cname);
    return page;

_ERR:
    if (NULL != view) {
        lisaui_free(view);
    }
    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (page->view) {
        page_view_t *view = (page_view_t *)page->view;

        ebus_message_unsubscribe(view->ebus_ch_base_event, event_inter_state_handler);

        if (view->inter) {
            
            lisa_ui_scr_base_del(view->inter);
            view->inter = NULL; 
        }

        lisaui_free(view);
        page->view = NULL;
    }

    /* 释放页面私有数据（如果有的话） */
    if (page->page_data) {
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }

    LISAUI_LOGI(TAG, "Destroy %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page)
{

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page %s view not created", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (view->inter) {
        LISAUI_USERDATA_WITH_LOCK(_userdata)
        {
            _userdata->setting.mic_is_mute = false;
            ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_MUTE_UPDATE, NULL, 0);
            if(_userdata->setting.mic_is_mute){
                lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set((lv_obj_t *)view->inter, lv_color_hex(0xDF8E02));
                lisa_ui_scr_ai_human_commu_btn_mute_img_set((lv_obj_t *)view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_mute));
            }
            else{
                lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set((lv_obj_t *)view->inter, lv_color_hex(0xFABE00));
                lisa_ui_scr_ai_human_commu_btn_mute_img_set((lv_obj_t *)view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_unmute));
            }
        }

        update_role_image(view);
        lisa_ui_scr_base_show(view->inter);
    }

    LISAUI_LOGI(TAG, "Show %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page %s view not created", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    extern lv_obj_t *lisa_ui_sys_bar_get(void);
    lv_obj_t *sys_bar = lisa_ui_sys_bar_get();
    lv_obj_set_style_bg_color(sys_bar, lv_color_hex(0xFFF6CC), LV_PART_MAIN);
    lisa_ui_scr_base_hide(view->inter);

    LISAUI_LOGI(TAG, "Close %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t *)data;
    
    if (!page || !data) {

        LISAUI_LOGE(TAG, "Invalid parameters for update");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view || !view->inter) {

        LISAUI_LOGE(TAG, "Page %s view not created or inter is NULL", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Update %s page data", page->cname);
    return LISAUI_ERR_OK;
}

static const lisaui_page_t luancher_commu_page = {
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_COMMU,
    .cname = "launcher.commu",
    .view = NULL,
    .page_data = NULL,
    .attribute =
        {
            .type = LISAUI_PAGE_TYPE_DATA_PAGE,
        },
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(luancher_commu_page);
