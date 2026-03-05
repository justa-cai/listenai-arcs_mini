/**
 * @file setting_wifi_view.h
 * @brief WiFi settings view
 */

#ifndef __SETTING_WIFI_VIEW_H__
#define __SETTING_WIFI_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lisa_ui_llm_base.h"

/** WiFi settings view class definition */
extern const lv_obj_class_t lisa_ui_setting_wifi_view_class;

/**
 * @brief 返回按钮点击回调函数类型
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_setting_wifi_back_cb_t)(void *user_data);

/**
 * @brief Create WiFi settings view
 * @param parent Parent object
 * @return lv_obj_t* Created view object
 */
lv_obj_t *lisa_ui_setting_wifi_view_create(lv_obj_t *parent);

/**
 * @brief Get WiFi switch object
 * @param obj WiFi view object
 * @return lv_obj_t* WiFi switch object
 */
lv_obj_t *lisa_ui_setting_wifi_view_get_switch(lv_obj_t *obj);

/**
 * @brief 设置返回按钮点击回调
 * @param obj WiFi view对象
 * @param cb 回调函数
 * @param user_data 传递给回调的用户数据
 */
void lisa_ui_setting_wifi_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_wifi_back_cb_t cb, void *user_data);

/**
 * @brief WiFi AP 信息结构
 */
typedef struct {
    char ssid[32];
    int rssi;
    int channel;
    bool is_connected;  /* 是否已连接 */
} lisa_ui_wifi_ap_info_t;

/**
 * @brief WiFi 项点击回调函数类型
 * @param ssid WiFi SSID
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_setting_wifi_item_cb_t)(const char *ssid, void *user_data);

/**
 * @brief WiFi 密码确认回调函数类型
 * @param ssid WiFi SSID
 * @param password WiFi 密码
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_setting_wifi_password_confirm_cb_t)(const char *ssid, const char *password, void *user_data);

/**
 * @brief WiFi 密码取消回调函数类型
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_setting_wifi_password_cancel_cb_t)(void *user_data);

/**
 * @brief 更新 WiFi 扫描列表
 * @param obj WiFi view对象
 * @param ap_list AP 信息数组
 * @param ap_count AP 数量
 */
void lisa_ui_setting_wifi_view_update_list(lv_obj_t *obj, const lisa_ui_wifi_ap_info_t *ap_list, int ap_count);

/**
 * @brief 设置 WiFi 项点击回调
 * @param obj WiFi view对象
 * @param cb 回调函数
 * @param user_data 传递给回调的用户数据
 */
void lisa_ui_setting_wifi_view_set_item_cb(lv_obj_t *obj, lisa_ui_setting_wifi_item_cb_t cb, void *user_data);

/**
 * @brief 清空 WiFi 列表
 * @param obj WiFi view对象
 */
void lisa_ui_setting_wifi_view_clear_list(lv_obj_t *obj);

/**
 * @brief 显示密码输入界面
 * @param obj WiFi view对象
 * @param ssid WiFi SSID
 * @param confirm_cb 确认回调
 * @param cancel_cb 取消回调
 * @param user_data 用户数据
 */
void lisa_ui_setting_wifi_view_show_password_input(lv_obj_t *obj, const char *ssid,
                                                    lisa_ui_setting_wifi_password_confirm_cb_t confirm_cb,
                                                    lisa_ui_setting_wifi_password_cancel_cb_t cancel_cb,
                                                    void *user_data);

/**
 * @brief 隐藏密码输入界面
 * @param obj WiFi view对象
 */
void lisa_ui_setting_wifi_view_hide_password_input(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* __SETTING_WIFI_VIEW_H__ */
