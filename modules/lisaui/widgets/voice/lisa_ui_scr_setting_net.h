#ifndef __LISA_UI_SCR_SETTING_NET_H__
#define __LISA_UI_SCR_SETTING_NET_H__

#include "lvgl.h"
/**
 * @brief WiFi连接回调
 *
 * @param ssid WiFi名称
 * @param pwd WiFi密码, 可能為空, 如果是已经连接过的WiFi, 则密码为空, 如果是开放wifi, 则密码为空
 * @param is_encrypted 是否加密
 * @param is_mine 是否是已经连接过的WiFi
 * @param user_data 用户数据
 */
typedef void (*lisa_ui_scr_setting_net_wifi_connecting_cb_t)(const char *ssid, const char *pwd, bool is_encrypted,
                                                             bool is_mine, void *user_data);

/**
 * @brief 创建WiFi设置界面
 *
 * @param parent 父对象
 * @return lv_obj_t* WiFi设置界面对象
 */
lv_obj_t *lisa_ui_scr_setting_net_create(lv_obj_t *parent);

/**
 * @brief 设置WiFi连接状态(已连接)
 *
 * @param obj WiFi列表项对象
 * @param txt WiFi名称
 */
void lisa_ui_scr_setting_net_wifi_item_connected(lv_obj_t *obj, const char *txt);

/**
 * @brief 设置WiFi连接状态(正在连接)
 *
 * @param obj WiFi列表项对象
 * @param txt WiFi名称
 */
void lisa_ui_scr_setting_net_wifi_item_connecting(lv_obj_t *obj, const char *txt);

/**
 * @brief 清空WiFi列表
 *
 * @param obj WiFi列表对象
 */
void lisa_ui_scr_setting_net_wifi_list_clear(lv_obj_t *obj);

/**
 * @brief 添加WiFi列表项到我的列表(已经连接过的WiFi)
 *
 * @param obj WiFi列表对象
 * @param txt WiFi名称
 * @param encrypted 是否加密
 * @param rssi_level 信号强度 1~5
 * @param connect_state 连接状态,0:未连接,1:连接中,2:已连接
 * @param cb WiFi连接回调
 * @param user_data 用户数据
 * @return lv_obj_t* WiFi列表项对象
 */
lv_obj_t *lisa_ui_wifi_item_add_to_list_mine(lv_obj_t *obj, const char *txt, bool encrypted,uint8_t rssi_level,uint8_t connect_state,
                                             lisa_ui_scr_setting_net_wifi_connecting_cb_t cb, void *user_data);

/**
 * @brief 添加WiFi列表项到其他列表(未连接过的WiFi)
 *
 * @param obj WiFi列表对象
 * @param txt WiFi名称
 * @param encrypted 是否加密
 * @param rssi_level 信号强度 1~5
 * @param connect_state 连接状态,0:未连接,1:连接中,2:已连接
 * @param cb WiFi连接回调
 * @param user_data 用户数据
 * @return lv_obj_t* WiFi列表项对象
 */
lv_obj_t *lisa_ui_wifi_item_add_to_list_other(lv_obj_t *obj, const char *txt, bool encrypted,uint8_t rssi_level,uint8_t connect_state,
                                              lisa_ui_scr_setting_net_wifi_connecting_cb_t cb, void *user_data);

/**
 * @brief 增加wifi开关状态
 *
 * @param obj WiFi列表对象
 * @param state 状态
 */
int lisa_ui_scr_setting_net_switch_add_state(const lv_obj_t *obj,lv_state_t state);

/**
 * @brief 清除wifi开关状态
 *
 * @param obj WiFi列表对象
 * @param state 状态
 */
int lisa_ui_scr_setting_net_switch_clear_state(const lv_obj_t *obj,lv_state_t state);

/**
 * @brief 判断是否正在连接
 *
 * @param obj WiFi列表对象
 * @return true 正在连接
 * @return false 未连接
 */
bool lisa_ui_scr_setting_net_is_connecting(lv_obj_t *obj);
#endif /* __LISA_UI_SCR_SETTING_NET_H__ */
