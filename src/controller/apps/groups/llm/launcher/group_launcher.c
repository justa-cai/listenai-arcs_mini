/**
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>

#include "lisaui_log.h"
#include "lisaui_type.h"
#include "assets/assets_res.h"
#include "lisaui_group.h"
#include "lisaui_group_base.h"
#include "lisaui_manager.h"
#include "platform.h"
#include "pages/launcher_pages.h"
#include "pages/page_info.h"
#include "user_groups.h"
#include "group_launcher.h"
#include "ebus/ebus.h"
#include "lisaui_user_data.h"
#include "lisa_ui_llm_primary.h"

static const char *TAG = "group.launcher";

// ebus通道全局变量
static ebus_chn_t *ebus_ch_base_event = NULL;

/**
 * @brief ebus事件处理函数 - 处理页面切换事件
 */
static int event_page_toggle_handler(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size, void *user_data)
{
    LISAUI_LOGI(TAG, "Received page toggle event via ebus, code: %d (CAMERA_SHOW=%d)", 
               code, LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_SHOW);
    
    switch (code) {
        case LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE:
            // 如果Info页面已激活，则刷新当前页面数据，避免重复入栈导致卡死
            if (is_info_page_active() && lisaui_manager_get_current_group() &&
                lisaui_manager_get_current_group()->current_page &&
                lisaui_manager_get_current_group()->current_page->page_index == LISAUI_GROUP_LAUNCHER_PAGE_INDEX_INFO) {
                lisaui_page_t *cur = lisaui_manager_get_current_group()->current_page;
                if (cur && cur->update_data) {
                    LISAUI_LOGI(TAG, "Info page active; refreshing data instead of re-entering");
                    cur->update_data(cur, NULL);
                } else {
                    // 回退到正常切换逻辑
                    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                        LISAUI_GROUP_LAUNCHER_PAGE_INDEX_INFO, LISAUI_PAGE_SAVE_TO_HISTORY);
                }
            } else {
                // 调用页面切换逻辑
                lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                    LISAUI_GROUP_LAUNCHER_PAGE_INDEX_INFO, LISAUI_PAGE_SAVE_TO_HISTORY);
            }
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE:
            lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, LISAUI_PAGE_SAVE_TO_HISTORY);
            break;
        case LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_SHOW:
            // 显示拍照图片
            {
                LISAUI_LOGI(TAG, "Camera image show event received, code=%d, msg_size=%d, message=%p", 
                            code, msg_size, message);
                lisaui_camera_image_params_t *params = (lisaui_camera_image_params_t *)message;
                if (params && msg_size == sizeof(lisaui_camera_image_params_t)) {
                    LISAUI_LOGI(TAG, "Showing camera image: params=%p, buffer=%p, %dx%d", 
                                params, params->rgb565_data, params->width, params->height);
                    
                    // 获取主页面UI对象
                    lv_obj_t *llm_ui = page_primary_get_ui_object();
                    if (llm_ui) {
                        if (params->rgb565_data) {
                            // 停止emoji动画
                            current_staged_emoji_animation_stop();
                        }
                        lisa_ui_llm_primary_show_camera_image(llm_ui, params->rgb565_data, 
                                                                params->width, params->height);
                    } else {
                        LISAUI_LOGW(TAG, "Primary page UI not available");
                    }
                } else {
                    LISAUI_LOGW(TAG, "Invalid params or msg_size: params=%p, msg_size=%d, expected=%zu",
                                params, msg_size, sizeof(lisaui_camera_image_params_t));
                }
                
                // 释放exram动态分配的内存
                if (params) {
                    exram_free(params);
                }
            }
            break;
        case LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_HIDE:
            // 隐藏拍照图片
            {
                LISAUI_LOGI(TAG, "Hiding camera image");
                
                // 获取主页面UI对象
                lv_obj_t *llm_ui = page_primary_get_ui_object();
                if (llm_ui) {
                    lisa_ui_llm_primary_hide_camera_image(llm_ui);
                } else {
                    LISAUI_LOGW(TAG, "Primary page UI not available");
                }
            }
            break;
        case LISAUI_EBUS_CH_EVENT_M2U_NET_IMAGE_SHOW:
            // 显示网络图片
            {
                LISAUI_LOGI(TAG, "Net image show event received, code=%d, msg_size=%d, message=%p",
                            code, msg_size, message);

                lisaui_net_image_params_t *params = (lisaui_net_image_params_t *)message;
                if (params && msg_size == sizeof(lisaui_net_image_params_t)) {
                    const lv_img_dsc_t *img_dsc = (const lv_img_dsc_t *)params->img_dsc;
                    if (img_dsc) {
                        LISAUI_LOGI(TAG, "Showing net image: params=%p, img_dsc=%p, size=%u", params, img_dsc,
                                    img_dsc->data_size);

                        lv_obj_t *llm_ui = page_primary_get_ui_object();
                        if (llm_ui) {
                            current_staged_emoji_animation_stop();
                            lisa_ui_llm_primary_show_net_image(llm_ui, img_dsc, true);
                        } else {
                            LISAUI_LOGW(TAG, "Primary page UI not available");
                        }
                    } else {
                        LISAUI_LOGW(TAG, "img_dsc is NULL");
                    }
                } else {
                    LISAUI_LOGW(TAG, "Invalid params or msg_size: params=%p, msg_size=%d, expected=%zu",
                                params, msg_size, sizeof(lisaui_net_image_params_t));
                }

                if (params) {
                    exram_free(params);
                }
            }
        break;

        case LISAUI_EBUS_CH_EVENT_M2U_MUSIC_COVER_SHOW:
            // 显示音乐封面和标题
            {
                LISAUI_LOGI(TAG, "Music cover show event received, code=%d, msg_size=%d, message=%p",
                            code, msg_size, message);

                lisaui_net_image_params_t *params = (lisaui_net_image_params_t *)message;
                if (params && msg_size >= sizeof(lisaui_net_image_params_t)) {
                    const lv_img_dsc_t *img_dsc = (const lv_img_dsc_t *)params->img_dsc;
                    if (img_dsc) {
                        LISAUI_LOGI(TAG, "Showing music cover: params=%p, img_dsc=%p, size=%u, title=%s",
                                    params, img_dsc, img_dsc->data_size,
                                    params->music_title ? params->music_title : "NULL");

                        lv_obj_t *llm_ui = page_primary_get_ui_object();
                        if (llm_ui) {
                            current_staged_emoji_animation_stop();
                            lisa_ui_llm_primary_show_music_cover(llm_ui, img_dsc, params->music_title);
                        } else {
                            LISAUI_LOGW(TAG, "Primary page UI not available");
                        }
                    } else {
                        LISAUI_LOGW(TAG, "img_dsc is NULL");
                    }
                } else {
                    LISAUI_LOGW(TAG, "Invalid params or msg_size: params=%p, msg_size=%d, expected=%zu",
                                params, msg_size, sizeof(lisaui_net_image_params_t));
                }

                if (params) {
                    exram_free(params);
                }
            }
        break;
        
        default:
            LISAUI_LOGW(TAG, "Unknown event code: %d", code);
            break;
    }
    
    return 0;
}

static lisaui_err_t group_launcher_setup(lisaui_group_t *group)
{
    //
        
    // 注册ebus事件处理
    ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (ebus_ch_base_event != NULL) {
        ebus_message_subscribe(ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE,
                        event_page_toggle_handler, 
                        NULL);
        ebus_message_subscribe(ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_SHOW,
                        event_page_toggle_handler, 
                        NULL);

        ebus_message_subscribe(ebus_ch_base_event,
                EBUS_SUBSCRIBER_TYPE_SYNC,
                LISAUI_EBUS_CH_EVENT_M2U_NET_IMAGE_SHOW,
                event_page_toggle_handler,
                NULL);

        ebus_message_subscribe(ebus_ch_base_event,
                EBUS_SUBSCRIBER_TYPE_SYNC,
                LISAUI_EBUS_CH_EVENT_M2U_MUSIC_COVER_SHOW,
                event_page_toggle_handler,
                NULL);
        
        ebus_message_subscribe(ebus_ch_base_event, 
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_M2U_CAMERA_IMAGE_HIDE,
                        event_page_toggle_handler, 
                        NULL);
        
        LISAUI_LOGI(TAG, "ebus channel subscribed successfully (info + alarm + home + camera events)");
    } else {
        LISAUI_LOGW(TAG, "failed to bind ebus channel");
    }
    
    return GROUP_BASE_SETUP_DEFAULT(group);
}

static lisaui_err_t group_launcher_cleanup(lisaui_group_t *group)
{
    return GROUP_BASE_CLEANUP_DEFAULT(group);
}

static lisaui_err_t group_launcher_enter(lisaui_group_t *group, lisaui_group_enter_page_method_t method, int page_index,
                                         uint32_t flags)
{
    return GROUP_BASE_ENTER_DEFAULT(group, method, page_index, flags);
}

static lisaui_err_t group_launcher_exit(lisaui_group_t *group)
{
    return GROUP_BASE_EXIT_DEFAULT(group);
}

static group_icon_t icon_res = {
    .hidden_icon = true,
    .title = "Launcher",
    .res = NULL,
    .zoom = GROUP_ICON_ZOOM(0),
};

static lisaui_group_t group_launcher = {
    .setup = group_launcher_setup,
    .cleanup = group_launcher_cleanup,
    .enter = group_launcher_enter,
    .exit = group_launcher_exit,
    .main_page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY,
    .info =
        {
            .name = "launcher",
            .package_name = "com.listenai.lisaui.launcher",
            .id = LISAUI_GROUP_INDEX_LAUNCHER,
            .type = LISAUI_GROUP_TYPE_LAUNCHER,
            .keep_in_stack = true,
        },
    .icon = &icon_res,

    .page_stack = NULL,
    .private_data = NULL,
};

static lisaui_err_t group_launcher_init(void)
{
    LISAUI_LOGI(TAG, "%s", __FUNCTION__);
    lisaui_manager_group_register(&group_launcher);
    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                               LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY,0);
    return LISAUI_ERR_OK;
}

LISAUI_GROUP_REGISTER(group_launcher, group_launcher_init, 1);
