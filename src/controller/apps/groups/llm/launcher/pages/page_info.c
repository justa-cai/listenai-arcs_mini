#include <stdint.h>
#include <string.h>

#include "ebus/ebus.h"
#include "lvgl.h"

#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "launcher_pages.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_assets.h"
#include "user_groups.h"
#include "lisaui_log.h"
#include "lisaui_user_data.h"
#include "lisa_display.h"
#include "lv_img_net_loader.h"

#include "../group_launcher.h"
#include "lisa_kv.h"
#include "../../../kv/kv_user.h"

#define TAG "launcher.page.info"
LV_IMG_DECLARE(ble_qr);
// 全局状态变量
static bool g_info_page_active = false;

// 函数声明
static void auto_return_timer_cb(lv_timer_t *timer);
bool is_info_page_active(void);
void set_info_page_active(bool active);
static int get_device_id(char *device_id, int max_len);

// 网络图片加载辅助函数
static bool is_network_url(const char* url) {
    if (!url) return false;
    return (strncmp(url, "http://", 7) == 0 || 
            strncmp(url, "https://", 8) == 0 ||
            strncmp(url, "N:", 2) == 0);
}

static lv_res_t set_qr_image_src(lv_obj_t* img, const char* url) {
    if (!img || !url) {
        LISAUI_LOGE(TAG, "Invalid parameters for QR image");
        return LV_RES_INV;
    }
    
    if (is_network_url(url)) {
        LISAUI_LOGI(TAG, "Loading network QR image: %s", url);
        return lv_img_set_src_net(img, url);
    } else {
        LISAUI_LOGI(TAG, "Using local QR image: %s", url);
        lv_img_set_src(img, url);
        return LV_RES_OK;
    }
}

/**
 * @brief Get device ID from KV storage or chip hardware ID
 *
 * @param device_id Buffer to store device ID
 * @param max_len Maximum buffer length
 * @return int 0 on success, -1 on failure
 */
static int get_device_id(char *device_id, int max_len)
{
    if (!device_id || max_len <= 0) {
        return -1;
    }
    char *kv_device_id = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_DEVICE_ID, &kv_device_id);

    if (ret == 0 && kv_device_id != NULL && strlen(kv_device_id) > 0) {
        if (strlen(kv_device_id) >= max_len) {
            lisa_kv_free(kv_device_id);
            return -1;
        }
        strcpy(device_id, kv_device_id);
        lisa_kv_free(kv_device_id);
        return 0;
    } else {
        // KV中没有设备ID，从芯片读取硬件ID
        uint32_t *id_1 = (uint32_t *)0x48600208;
        uint32_t *id_2 = (uint32_t *)0x4860020c;
        uint8_t id_buffer[8];

        if (*id_1 == 0 && *id_2 == 0) {
            // 芯片ID也是全零，返回错误
            return -1;
        }

        if (max_len < 17) { // 16个字符 + 结束符
            return -1;
        }

        memcpy(id_buffer, id_1, sizeof(uint32_t));
        memcpy(id_buffer + 4, id_2, sizeof(uint32_t));

        snprintf(device_id, max_len, "%02x%02x%02x%02x%02x%02x%02x%02x",
                id_buffer[0], id_buffer[1], id_buffer[2], id_buffer[3],
                id_buffer[4], id_buffer[5], id_buffer[6], id_buffer[7]);

        return 0;
    }
}

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *label;
    lv_obj_t *g_ble_qr_img;
    lv_obj_t *bottom_label;                                             // 添加底部文本标签
    lv_obj_t *device_id_label;                                          // 设备ID标签
    lv_timer_t *auto_return_timer;
    ebus_chn_t *ebus_ch_base_event;
    lisaui_userdata_qrcode_inter_mode_e mode;
} page_view_t;

static int update_wifi_state(page_view_t *view)
{
    if (!view) {
        LISAUI_LOGE(TAG, "Invalid view parameter");
        return -1;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        uint8_t connect_state = _userdata->setting.wifi.connect_info.state;
        lisaui_userdata_qrcode_inter_mode_e mode = _userdata->qrcode_inter.mode;

        LISAUI_LOGI(TAG, "WiFi state update: mode=%d, connect_state=%d", mode, connect_state);

        switch (mode) {
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK:
            if (connect_state == LISAUI_USERDATA_WIFI_CONNECT_STATE_CONNECTED) {
                LISAUI_LOGI(TAG, "WiFi connected, returning to home page");
                lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, 
                                         GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                         LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 
                                         0);
            }
            break;

        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE:
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA:
            LISAUI_LOGD(TAG, "Mode %d: WiFi state change ignored", mode);
            break;

        default:
            LISAUI_LOGW(TAG, "Unknown qrcode inter mode: %d", mode);
            return -1;
        }
    }

    return 0;
}

static int update_wakeup_state(page_view_t *view)
{
    // 检查当前页面是否处于活跃状态，避免与其他页面冲突
    if (!is_info_page_active()) {
        LISAUI_LOGD(TAG, "Info page not active, ignoring wakeup event");
        return 0;
    }
    
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        lisaui_userdata_qrcode_inter_mode_e mode = _userdata->qrcode_inter.mode;
        
        LISAUI_LOGI(TAG, "Wakeup state update: mode=%d", mode);

        switch (mode) {
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA:
            LISAUI_LOGI(TAG, "Wakeup during the configure quota, returning to home page");
            lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, 
                                        GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                        LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 
                                        0);
            break;

        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE:
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK:
            LISAUI_LOGD(TAG, "Mode %d: Wakeup event handled by staying in current page", mode);
            break;

        default:
            LISAUI_LOGW(TAG, "Unknown qrcode inter mode: %d", mode);
            return -1;
        }
    }
    
    return 0;
}

static int event_inter_state_handler(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size, void *user_data)
{
    page_view_t *view = (page_view_t *)user_data;

    if (view == NULL) {
        return -1;
    }

    LISAUI_LOGI(TAG, "event_inter_state_handler, code:%d", code);

    switch (code) {
    case LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE:
        update_wifi_state(view);
        break;

    case LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP:
        update_wakeup_state(view);
        break;
        
    default:
        LISAUI_LOGW(TAG, "Unknown event code: %d", code);
        break;
    }

    return 0;
}

static lisaui_page_t *create(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "create info page");


    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if (!view) {
        LISAUI_LOGE(TAG, "failed to allocate page view");
        return NULL;
    }
    memset(view, 0, sizeof(page_view_t));

    // 创建屏幕对象
    view->screen = lv_obj_create(NULL);
    if (!view->screen) {
        LISAUI_LOGE(TAG, "failed to create screen object");
        lisaui_free(view);
        return NULL;
    }

    // 验证创建的对象
    if (!lv_obj_is_valid(view->screen)) {
        LISAUI_LOGE(TAG, "created screen object is invalid");
        lisaui_free(view);
        return NULL;
    }

    // 设置屏幕为全屏黑色背景
    lv_obj_set_size(view->screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(view->screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(view->screen, 0, 0);
    
    // 设置屏幕为垂直flex布局
    lv_obj_set_style_flex_flow(view->screen, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_align(view->screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 创建文本标签
    view->label = lv_label_create(view->screen);
    if (!view->label) {
        LISAUI_LOGE(TAG, "failed to create label object");
        lv_obj_del(view->screen);
        lisaui_free(view);
        return NULL;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        view->mode = _userdata->qrcode_inter.mode;
    }

    // 设置标签属性
    lv_obj_set_style_text_color(view->label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(view->label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(view->label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->label, LV_PCT(90));
    lv_obj_set_height(view->label, 40);
    lv_obj_align(view->label, LV_ALIGN_CENTER, 0, 0);

    // 创建图片对象
    view->g_ble_qr_img = lv_img_create(view->screen);
    lv_obj_set_style_pad_top(view->g_ble_qr_img, 0, 0);

    // 创建底部文本标签
    view->bottom_label = lv_label_create(view->screen);
    if (!view->bottom_label) {
        LISAUI_LOGE(TAG, "failed to create bottom label object");
        lv_obj_del(view->screen);
        lisaui_free(view);
        return NULL;
    }

    // 设置底部标签属性
    lv_obj_set_style_text_color(view->bottom_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->bottom_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(view->bottom_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(view->bottom_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->bottom_label, LV_PCT(90));
    lv_obj_set_height(view->bottom_label, 40);
    lv_obj_set_style_pad_top(view->bottom_label, 0, 0);
    lv_obj_add_flag(view->bottom_label, LV_OBJ_FLAG_HIDDEN);

    // 创建设备ID标签
    view->device_id_label = lv_label_create(view->screen);
    if (!view->device_id_label) {
        LISAUI_LOGE(TAG, "failed to create device ID label object");
        lv_obj_del(view->screen);
        lisaui_free(view);
        return NULL;
    }

    // 设置设备ID标签属性
    lv_obj_set_style_text_color(view->device_id_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->device_id_label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(view->device_id_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(view->device_id_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->device_id_label, LV_PCT(90));
    lv_obj_set_height(view->device_id_label, 30);
    lv_obj_set_style_pad_top(view->device_id_label, -7, 0);  // 负值向上移动
    lv_obj_add_flag(view->device_id_label, LV_OBJ_FLAG_HIDDEN);

    switch (view->mode) {
    case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK:
        lv_label_set_text(view->label, "当前网络未连接\n请使用微信扫码进行配网");
        lv_img_set_src(view->g_ble_qr_img, &ble_qr);

        // 获取并显示设备ID
        char device_id[32] = {0};
        if (get_device_id(device_id, sizeof(device_id)) == 0) {
            char device_id_text[64];
            snprintf(device_id_text, sizeof(device_id_text), "ID:%s", device_id);//设备ID:
            lv_label_set_text(view->device_id_label, device_id_text);
            lv_obj_clear_flag(view->device_id_label, LV_OBJ_FLAG_HIDDEN);
            LISAUI_LOGI(TAG, "Display device ID: %s", device_id);
        } else {
            LISAUI_LOGW(TAG, "Failed to get device ID");
        }
        break;
    case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE:
        lv_obj_set_height(view->label, 20);
        // 使用设备配置专用的文本和URL
        LISAUI_USERDATA_WITH_LOCK(_userdata) {
            if (_userdata->qrcode_inter.device_label_text && strlen(_userdata->qrcode_inter.device_label_text) > 0) {
                lv_label_set_text(view->label, _userdata->qrcode_inter.device_label_text);
            } else {
                lv_label_set_text(view->label, "请使用微信扫码进行配置");
            }
            
            if (_userdata->qrcode_inter.device_url && strlen(_userdata->qrcode_inter.device_url) > 0) {
                if (set_qr_image_src(view->g_ble_qr_img, _userdata->qrcode_inter.device_url) != LV_RES_OK) {
                    LISAUI_LOGE(TAG, "Failed to set QR code image: %s", _userdata->qrcode_inter.device_url);
                }
            } else {
                LISAUI_LOGE(TAG, "QR code resource does not exist");
            }
        }
        break;
    case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA:
        lv_obj_set_height(view->label, 15);
        lv_obj_set_style_max_height(view->g_ble_qr_img, 180, 0);
        
        // 使用配额模式专用的文本和URL
        LISAUI_USERDATA_WITH_LOCK(_userdata) {
            // 设置错误消息文本，如果没有则使用默认文本
            if (_userdata->qrcode_inter.quota_label_text && strlen(_userdata->qrcode_inter.quota_label_text) > 0) {
                lv_label_set_text(view->label, _userdata->qrcode_inter.quota_label_text);
            } else {
                lv_label_set_text(view->label, "今日交互额度已用完");
            }
            
            // 设置二维码图片源，如果有URL则使用URL，否则使用默认二维码
            if (_userdata->qrcode_inter.quota_url && strlen(_userdata->qrcode_inter.quota_url) > 0) {
                // 使用网络URL生成二维码
                
                if (set_qr_image_src(view->g_ble_qr_img, _userdata->qrcode_inter.quota_url) == LV_RES_OK) {
                    LISAUI_LOGI(TAG, "Using dynamic URL for QR code: %s", _userdata->qrcode_inter.quota_url);
                } else {
                    LISAUI_LOGE(TAG, "Failed to load QR code from URL: %s", _userdata->qrcode_inter.quota_url);
                }
            } else {
                LISAUI_LOGE(TAG, "QR code resource does not exist");
            }
        }
        
        lv_label_set_text(view->bottom_label, "微信扫码开通会员\n获取更多交互额度");
        lv_obj_clear_flag(view->bottom_label, LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        LISAUI_LOGI(TAG, "Unknow qrcode inter mode, mode: %d", view->mode);
        break;
    }

    // 初始化定时器为NULL
    view->auto_return_timer = NULL;
    
    view->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (view->ebus_ch_base_event != NULL) {
        ebus_message_subscribe( view->ebus_ch_base_event ,
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE,
                        NULL, 
                        view);

        ebus_message_subscribe( view->ebus_ch_base_event ,
                EBUS_SUBSCRIBER_TYPE_SYNC, 
                LISAUI_EBUS_CH_EVENT_M2U_SETTING_WIFI_UPDATE,
                event_inter_state_handler, 
                view);

        ebus_message_subscribe( view->ebus_ch_base_event,
            EBUS_SUBSCRIBER_TYPE_SYNC,
            LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP,
            event_inter_state_handler,
            view);
    }
    page->view = view;
    LISAUI_LOGI(TAG, "info page created successfully %p", view->screen);
    //
    LISAUI_LOGI(TAG, "info page created successfully!!! %p", view->screen);
    return page;
}
static lisaui_err_t destroy(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "destroy info page");

    if (page && page->view) {
        page_view_t *view = (page_view_t *)page->view;

        // 取消事件总线订阅
        if (view->ebus_ch_base_event) {
            ebus_message_unsubscribe(view->ebus_ch_base_event, event_inter_state_handler);
            view->ebus_ch_base_event = NULL;
            LISAUI_LOGI(TAG, "Event bus unsubscribed");
        }
        
        // 清理定时器
        if (view->auto_return_timer) {
            lv_timer_del(view->auto_return_timer);
            view->auto_return_timer = NULL;
        }

        if (view->screen) {
            lv_obj_del(view->screen);
        }
        lisaui_free(view);
        page->view = NULL;
    }

    return LISAUI_ERR_OK;
}

// 自动返回定时器回调函数
static void auto_return_timer_cb(lv_timer_t *timer)
{
    LISAUI_LOGI(TAG, "Auto return timer expired, returning to home page");
    
    // 重置状态
    g_info_page_active = false;
    
    // 返回主页面
    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                               LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 0);
   
    // 定时器会被自动删除，因为是一次性定时器
}

void _ui_screen_change(lv_obj_t *target, lv_scr_load_anim_t fademode, int spd, int delay)
{
    lv_scr_load_anim(target, fademode, spd, delay, false);
}

static lisaui_err_t show(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "show info page ,%p", page);

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (!view->screen) {
        LISAUI_LOGE(TAG, "Screen object is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 加载屏幕前进行额外验证
    if (!lv_obj_is_valid(view->screen)) {
        LISAUI_LOGE(TAG, "Screen object is invalid before loading");
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 设置页面激活状态
    set_info_page_active(true);

    // 使用延时加载避免竞态条件
    lv_scr_load_anim(view->screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    
    // 启动10秒自动返回定时器
    if ((!view->auto_return_timer) && (view->mode == LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE)) {
        view->auto_return_timer = lv_timer_create(auto_return_timer_cb, 10000, NULL);
        lv_timer_set_repeat_count(view->auto_return_timer, 1); // 一次性定时器
        LISAUI_LOGI(TAG, "Auto return timer started (10s)");
    }
    
    LISAUI_LOGI(TAG, "info page shown");

    return LISAUI_ERR_OK;
}
static lisaui_err_t close(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "close info page");

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 清理定时器并重置状态
    if (view->auto_return_timer) {
        lv_timer_del(view->auto_return_timer);
        view->auto_return_timer = NULL;
        LISAUI_LOGI(TAG, "Auto return timer deleted");
    }
    
    // 重置页面状态
    g_info_page_active = false;
    ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE, NULL, 0);
    // 页面关闭时不需要特殊处理，框架会自动切换屏幕
    LISAUI_LOGI(TAG, "info page closed");

    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{
    if (!page || !page->view) {
        LISAUI_LOGE(TAG, "Invalid page or view");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    
    // 根据不同模式更新对应的动态内容
    LISAUI_USERDATA_WITH_LOCK(_userdata) {
        switch (view->mode) {
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK:
            // 更新网络配置页面内容
            if (_userdata->qrcode_inter.network_label_text && strlen(_userdata->qrcode_inter.network_label_text) > 0) {
                lv_label_set_text(view->label, _userdata->qrcode_inter.network_label_text);
                LISAUI_LOGI(TAG, "Updated network message: %s", _userdata->qrcode_inter.network_label_text);
            }
            if (_userdata->qrcode_inter.network_url && strlen(_userdata->qrcode_inter.network_url) > 0) {
                if (set_qr_image_src(view->g_ble_qr_img, _userdata->qrcode_inter.network_url) == LV_RES_OK) {
                    LISAUI_LOGI(TAG, "Updated network QR code URL: %s", _userdata->qrcode_inter.network_url);
                } else {
                    LISAUI_LOGE(TAG, "Failed to update network QR code URL: %s", _userdata->qrcode_inter.network_url);
                }
            }
            break;
            
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE:
            // 更新设备配置页面内容
            if (_userdata->qrcode_inter.device_label_text && strlen(_userdata->qrcode_inter.device_label_text) > 0) {
                lv_label_set_text(view->label, _userdata->qrcode_inter.device_label_text);
                LISAUI_LOGI(TAG, "Updated device message: %s", _userdata->qrcode_inter.device_label_text);
            }
            if (_userdata->qrcode_inter.device_url && strlen(_userdata->qrcode_inter.device_url) > 0) {
                if (set_qr_image_src(view->g_ble_qr_img, _userdata->qrcode_inter.device_url) == LV_RES_OK) {
                    LISAUI_LOGI(TAG, "Updated device QR code URL: %s", _userdata->qrcode_inter.device_url);
                } else {
                    LISAUI_LOGE(TAG, "Failed to update device QR code URL: %s", _userdata->qrcode_inter.device_url);
                }
            }
            break;
            
        case LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_QUOTA:
            // 更新配额页面内容
            if (_userdata->qrcode_inter.quota_label_text && strlen(_userdata->qrcode_inter.quota_label_text) > 0) {
                lv_label_set_text(view->label, _userdata->qrcode_inter.quota_label_text);
                LISAUI_LOGI(TAG, "Updated quota error message: %s", _userdata->qrcode_inter.quota_label_text);
            }
            if (_userdata->qrcode_inter.quota_url && strlen(_userdata->qrcode_inter.quota_url) > 0) {
                if (set_qr_image_src(view->g_ble_qr_img, _userdata->qrcode_inter.quota_url) == LV_RES_OK) {
                    LISAUI_LOGI(TAG, "Updated quota QR code URL: %s", _userdata->qrcode_inter.quota_url);
                } else {
                    LISAUI_LOGE(TAG, "Failed to update quota QR code URL: %s", _userdata->qrcode_inter.quota_url);
                }
            }
            break;
            
        default:
            LISAUI_LOGW(TAG, "Unknown mode for data update: %d", view->mode);
            break;
        }
    }
    
    return LISAUI_ERR_OK;
}

lisaui_page_t page_info = {
    .cname = "page_info",
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_INFO,
    .view = NULL,
    .page_data = NULL,
    .attribute =
        {
            .type = LISAUI_PAGE_TYPE_DATA_PAGE,
        },
    .flags = 0,
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

// 提供给外部模块的接口函数
bool is_info_page_active(void)
{
    return g_info_page_active;
}

void set_info_page_active(bool active)
{
    g_info_page_active = active;
    LISAUI_LOGI(TAG, "Info page active state set to: %d", active);
}

LISAUI_PAGE_EXPORT(page_info);