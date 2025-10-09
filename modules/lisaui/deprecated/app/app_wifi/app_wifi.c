/**
 * @file app_wifi.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_wifi.h"
#include "app_wifi/wifi_type.h"
#include "lisaui_app_common.h"
#include "assets/assets_res.h"
#include "widget/ls_wifi_list.h"

#include "lv_img_utils.h"
#include "lisaui_log.h"
#include <src/core/lv_obj_pos.h>
#include <src/misc/lv_area.h>

#include "widget/ls_wifi_item.h"
#include "widget/ls_wifi_connect.h"

static const char *TAG = "app_wifi";
static lv_obj_t *g_app_wifi = NULL;
static lv_obj_t *g_app_panel = NULL;
lv_obj_t *g_obj_wifi_list = NULL;
static lv_img_dsc_t img_gif_anim_circle;

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_wifi/" path
#else
// #define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_wifi/" path
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_wifi/" path
// app/llm/lisaui/app/app_wifi/assets/gif/anim_circle.gif
#endif

UI_RES_IMG_NAME(anim_circle, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/gif/anim_circle_24x24.gif"))
UI_RES_IMG_NAME(wifi_refresh_icon, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/png/ic_refresh.png"))
lv_obj_t *LV_OBJ_ICON(refresh_static);
lv_obj_t *refresh_icon = NULL;

static app_wifi_event_handler_t g_app_wifi_event_handler = NULL;
lisaui_err_t lisaui_app_wifi_register_event_handler(app_wifi_event_handler_t handler)
{
    if (handler == NULL) {
        LISAUI_LOGE(TAG, "handler is NULL");
        return LISAUI_ERR_FAIL;
    }
    g_app_wifi_event_handler = handler;
    return LISAUI_ERR_OK;
}

static void ls_wifi_cancel_evt_handle(lv_obj_t *obj)
{
    lv_obj_del(obj);
}

static void ls_wifi_connect_evt_handle(lv_obj_t *obj, const char *ssid, const char *psw)
{
    LISAUI_LOGI(TAG, "wifi connect event, ssid:%s, psw:%s", ssid, psw);

    if (strlen(psw) < 8) {
        lisaui_popup_toast("密码长度至少8位");
        return;
    }

    if (g_obj_wifi_list) {
        /* 这里为了方便, 直接交换显示 */
        lisa_ui_wifi_item_t *item = ls_wifi_list_item_get_by_ssid(g_obj_wifi_list, ssid);
        lisa_ui_wifi_item_t *item1 = ls_wifi_list_item_get_by_index(g_obj_wifi_list, 0);

        if (item1 != item) {
            const char *text = lv_label_get_text(item1->ssid_lbl);
            ls_ui_wifi_item_set_ssid(item, text);
            ls_ui_wifi_item_set_status(item, "");

            ls_ui_wifi_item_set_ssid(item1, ssid);
            ls_ui_wifi_item_set_status(item1, "连接中");
            /* 列表滚动到顶部显示 */
            lv_obj_scroll_to(g_obj_wifi_list, 0, 0, LV_ANIM_ON);
        } else {
            /* 该item已经在顶部 */
            ls_ui_wifi_item_set_status(item, "连接中");
        }
    }

    /* send a wifi connect request */
    if (g_app_wifi_event_handler) {
        wifi_metadata_t *wifi_item = lisaui_malloc(sizeof(wifi_metadata_t));
        if (wifi_item) {
            memset(wifi_item, 0, sizeof(wifi_metadata_t));
            strncpy(wifi_item->SSID, ssid, LISAUI_APP_WIFI_SSID_MAX_LEN);
            strncpy(wifi_item->PWD, psw, LISAUI_APP_WIFI_PWD_MAX_LEN);
            g_app_wifi_event_handler(LISAUI_WIFI_ITEM_AP, LISAUI_WIFI_OP_CONNECT, wifi_item);
            lisaui_free(wifi_item);
        } else {
            LISAUI_LOGE(TAG, "malloc wifi_item failed");
        }
    }

    lv_obj_del(obj);
}

static void ls_wifi_item_evt_handler(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lisa_ui_wifi_item_t *obj = (lisa_ui_wifi_item_t *)lv_event_get_target(event);

    if (code == LV_EVENT_CLICKED) {
        ls_ui_wifi_connect_t *conn_scr = (ls_ui_wifi_connect_t *)ls_wifi_connect_create(lv_layer_sys());
        ls_wifi_connect_set_tips(conn_scr, lv_label_get_text(obj->ssid_lbl));
        ls_wifi_connect_set_cancel_cb(conn_scr, ls_wifi_cancel_evt_handle);
        ls_wifi_connect_set_connect_cb(conn_scr, ls_wifi_connect_evt_handle);
    }
}

lisaui_err_t lisaui_app_add_wifi_list_item(lv_obj_t *list, wifi_metadata_t *wifi_info)
{
    lisa_ui_wifi_item_t *wifi_item = NULL;
    char *status = NULL;

    if (wifi_info->status == LISAUI_WIFI_STATUS_CONNECTING) {
        status = "连接中";
    } else if (wifi_info->status == LISAUI_WIFI_STATUS_CONNECTED) {
        status = "已连接";
    } else if (wifi_info->status == LISAUI_WIFI_STATUS_DISCONNECT) {
        status = "";
    }

    wifi_item = ls_wifi_list_add_item(list, wifi_info->SSID, status);
    if (wifi_item == NULL) {
        return -1;
    }

    if (wifi_info->status != LISAUI_WIFI_STATUS_CONNECTED) {
        lv_obj_add_event_cb((lv_obj_t *)wifi_item, ls_wifi_item_evt_handler, LV_EVENT_CLICKED, NULL);
    }

    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_del_wifi_list(void)
{
    if (g_obj_wifi_list == NULL) {
        return LISAUI_ERR_OK;
    }

    // FIXME：销毁旧的列表
    return LISAUI_ERR_OK;
}


static void btn_refresh_event_handler(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);
    if (code == LV_EVENT_CLICKED) {
        LISAUI_LOGI(TAG, "button refresh clicked");
        if (g_app_wifi_event_handler) {
            g_app_wifi_event_handler(LISAUI_WIFI_ITEM_NONE, LISAUI_WIFI_OP_SCAN, NULL);
            lv_obj_add_flag(LV_OBJ_ICON(refresh_static), LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(refresh_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

lv_obj_t *app_wifi_create_panel_wifi_list(lv_obj_t *parent)
{

    lv_obj_t *wifi_list_panel = lv_obj_create(parent);
    _lisaui_set_style_container(wifi_list_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    lv_obj_t *refresh_btn = lv_btn_create(wifi_list_panel);
    lv_obj_set_width(refresh_btn, LV_DPX(200));
    // lv_obj_align(refresh_btn, LV_ALIGN_BOTTOM_MID, LV_DPX(80), -LV_DPX(30));
    // lv_obj_align_to(refresh_btn, password, LV_ALIGN_OUT_BOTTOM_MID, LV_DPX(80), LV_DPX(30));
    // lv_obj_add_event_cb(refresh_btn, msgbox_event_handler, LV_EVENT_CLICKED, (void *)wifi_item);
    // lv_obj_set_size(refresh_btn, LV_DPX(100), LV_SIZE_CONTENT);
    lv_obj_align(refresh_btn, LV_ALIGN_TOP_LEFT, LV_DPX(20), LV_DPX(LISAUI_STATUS_BAR_HEIGHT));
    // lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x24242d), 0);
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(refresh_btn, 8, 0);
    lv_obj_set_style_shadow_width(refresh_btn, 0, 0);
    lv_obj_set_style_border_width(refresh_btn, 0, 0);
    lv_obj_set_style_outline_width(refresh_btn, 0, 0);
    lv_obj_set_style_pad_all(refresh_btn, 4, 0);
    lv_obj_add_event_cb(refresh_btn, btn_refresh_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label;
    label = lv_label_create(refresh_btn);
    lv_label_set_text(label, "刷新");
    lv_obj_set_width(label, LV_PCT(50));
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    refresh_icon = lv_gif_create(refresh_btn);
    lv_img_gif_src_init(&img_gif_anim_circle, gfile_anim_circleData, gfile_anim_circleSize);
    lv_gif_set_src(refresh_icon, &img_gif_anim_circle);
    lv_obj_align_to(refresh_icon, label, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    lv_obj_add_flag(refresh_icon, LV_OBJ_FLAG_HIDDEN);

    // lv_obj_t *refresh_icon_static
    LV_OBJ_ICON(refresh_static) = lv_img_create(refresh_btn);
    lv_img_png_src_init(UI_RES_IMG_PNG(wifi_refresh_icon));
    lv_img_set_src(LV_OBJ_ICON(refresh_static), &LV_IMG_DSC(wifi_refresh_icon));
    lv_obj_align_to(LV_OBJ_ICON(refresh_static), label, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    // lv_obj_add_flag(refresh_icon, LV_OBJ_FLAG_HIDDEN);

    g_app_panel = lv_obj_create(wifi_list_panel);
    _lisaui_set_style_container(g_app_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    // LISAUI_COMMON_SET_APP_VIEW_PANEL_SIZE(wifi_list_panel, g_app_panel, NULL);
    lv_coord_t offset_y = 40;
    lv_obj_set_y(g_app_panel, LV_DPX(LISAUI_STATUS_BAR_HEIGHT + offset_y));
    lv_coord_t height = lv_obj_get_height(wifi_list_panel) - LV_DPX(LISAUI_STATUS_BAR_HEIGHT + offset_y);
    lv_obj_set_size(g_app_panel, LV_PCT(100), height);
    return wifi_list_panel;
}

lisaui_err_t lisaui_app_wifi_update_wifi_list(lisaui_wifi_list_t *wifi_list)
{
    lv_obj_clear_flag(LV_OBJ_ICON(refresh_static), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(refresh_icon, LV_OBJ_FLAG_HIDDEN);

    /* 清除旧的列表 */
    if (g_obj_wifi_list) {
        lv_obj_clean(g_obj_wifi_list);
        g_obj_wifi_list = NULL;
    }

    if (g_obj_wifi_list == NULL) {
        g_obj_wifi_list = ls_wifi_list_create(g_app_panel);
        lv_obj_set_size(g_obj_wifi_list, LV_PCT(100), LV_PCT(100));
    }

    int dis_cnt = 0;

    for (int i = 0; i < wifi_list->count && dis_cnt < LISAUI_APP_WIFI_LIST_ITEM_MAX; i++) {
        LISAUI_LOGI(TAG, "idx: %d, ssid:%s", i, wifi_list->list[i].SSID);
        if (wifi_list->list[i].SSID == NULL || strlen(wifi_list->list[i].SSID) == 0) {
            continue;
        }

        bool is_duplicate = false;
        for (int j = 0; j < i; j++) {
            if (strncmp(wifi_list->list[i].SSID, wifi_list->list[j].SSID, LISAUI_APP_WIFI_SSID_MAX_LEN) == 0) {
                is_duplicate = true;
                break;
            }
        }
        if (is_duplicate) {
            LISAUI_LOGI(TAG, "Duplicate wifi item: %s", wifi_list->list[i].SSID);
            continue;
        }

        lisaui_app_add_wifi_list_item(g_obj_wifi_list, &wifi_list->list[i]);

        dis_cnt++;
    }

    return LISAUI_ERR_OK;
}

static void wifi_list_pre_process(lisaui_wifi_list_t *old)
{
    /* 找到有连接状态的item, 移动到最前面 */
    bool found = false;
    int i;
    for (i = 0; i < old->count; i++) {
        if (old->list[i].status != LISAUI_WIFI_STATUS_DISCONNECT) {
            LISAUI_LOGI(TAG, "Fond wifi item: %s, status: %d", old->list[i].SSID, old->list[i].status);
            if (i != 0) {
                wifi_metadata_t *tmp = lisaui_malloc(sizeof(wifi_metadata_t));
                if (tmp) {
                    memcpy(tmp, &old->list[i], sizeof(wifi_metadata_t));
                    memcpy(&old->list[i], &old->list[0], sizeof(wifi_metadata_t));
                    memcpy(&old->list[0], tmp, sizeof(wifi_metadata_t));
                    lisaui_free(tmp);
                }
            }
            break;
        }
    }
}

lisaui_err_t lisaui_app_update_state(LISAUI_WIFI_STATE_e state, lisaui_wifi_list_t *hotspot_list)
{
    lisaui_app_del_wifi_list();

    LISAUI_LOGI(TAG, "[%s %d]Update wifi list:%p, state:%d,count:%d", __FUNCTION__, __LINE__, hotspot_list, state,
                hotspot_list->count);

    wifi_list_pre_process(hotspot_list);

    lisaui_wifi_list_t *wifi_list = hotspot_list;

    LVGL_UI_LOCK();
    lisaui_app_wifi_update_wifi_list(wifi_list);
    LVGL_UI_UNLOCK();

    return 0;
}

lisaui_err_t app_wifi_create(void *parent)
{

    if (g_app_wifi != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_wifi = app_wifi_create_panel_wifi_list(parent);
    LISAUI_LOGI(TAG, "[%d:%s] create\n", __LINE__, __func__);

    if (g_app_wifi_event_handler) {
        g_app_wifi_event_handler(LISAUI_WIFI_ITEM_NONE, LISAUI_WIFI_OP_SCAN, NULL);
        lv_obj_add_flag(LV_OBJ_ICON(refresh_static), LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(refresh_icon, LV_OBJ_FLAG_HIDDEN);
    }
    return LISAUI_ERR_OK;
}

lisaui_err_t app_wifi_destroy(void)
{
    LVGL_OBJ_SAFE_DEL(g_app_wifi);

    LISAUI_LOGI(TAG, "[%d:%s] destroy\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_wifi_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter\n", __LINE__, __func__);

    if (g_app_wifi_event_handler) {
        lisaui_wifi_list_t *wifi_list =
            lisaui_malloc(sizeof(lisaui_wifi_list_t) + sizeof(wifi_metadata_t) * LISAUI_APP_WIFI_LIST_ITEM_MAX);
        if (wifi_list == NULL) {
            LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
            return LISAUI_ERR_NO_MEMORY;
        }
        memset(wifi_list, 0, sizeof(lisaui_wifi_list_t) + sizeof(wifi_metadata_t) * LISAUI_APP_WIFI_LIST_ITEM_MAX);
        wifi_list->count = LISAUI_APP_WIFI_LIST_ITEM_MAX;
        g_app_wifi_event_handler(LISAUI_WIFI_ITEM_AP_LIST, LISAUI_WIFI_OP_GET_AP_LIST, (void *)wifi_list);
        lisaui_app_wifi_update_wifi_list(wifi_list);
        lisaui_free(wifi_list);
    }
    return LISAUI_ERR_OK;
}

lisaui_err_t app_wifi_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_wifi_get_view(void)
{
    return g_app_wifi;
}

static struct app_icon_t app_icon_res_wifi = {
    .title = "WIFI",
    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = &icon_img_app_wifi_png,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_wifi = {
    .create = app_wifi_create,
    .destroy = app_wifi_destroy,
    .enter = app_wifi_enter,
    .exit = app_wifi_exit,

    .get_app_view = app_wifi_get_view,
    .info =
        {
            .name = "Wifi",
            .package_name = "com.listenai.lisaui.wifi",
            .id = UI_APP_ID_WIFI,
        },
    .icon = &app_icon_res_wifi,
};

lisaui_err_t app_wifi_init(void)
{
    lisaui_app_register(&app_wifi);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(wifi, &app_wifi, app_wifi_init);
