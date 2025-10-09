/**
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LISAUI_MANAGER_H__
#define __LISAUI_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

// 包含基础类型定义
#include "lisaui_group.h"

#define LISAUI_PAGE_SAVE_TO_HISTORY         (1 << 0)

/*GROUP 注册事件*/
#define LISAUI_MANAGER_EVENT_GROUP_REGISTER     (1 << 0)
#define LISAUI_MANAGER_EVENT_GROUP_UNREGISTER   (1 << 1)
#define LISAUI_MANAGER_EVENT_GROUP_ENTER        (1 << 2)
#define LISAUI_MANAGER_EVENT_GROUP_EXIT         (1 << 3)
#define LISAUI_MANAGER_EVENT_GROUP_SWITCH_PAGE   (1 << 4)

/*系统按键点击事件*/
#define LISAUI_MANAGER_EVENT_SYS_BUTTON_HOME_CLICKED        (1 << 10)
#define LISAUI_MANAGER_EVENT_SYS_BUTTON_BACK_CLICKED        (1 << 11)
#define LISAUI_MANAGER_EVENT_SYS_BUTTON_CLOSE_CLICKED        (1 << 12)
#define LISAUI_MANAGER_EVENT_SYS_ALL                        (LISAUI_MANAGER_EVENT_SYS_BUTTON_HOME_CLICKED | \
                                                            LISAUI_MANAGER_EVENT_SYS_BUTTON_BACK_CLICKED | \
                                                            LISAUI_MANAGER_EVENT_SYS_BUTTON_CLOSE_CLICKED)


typedef int (*lisaui_manager_event_cb_t)(uint32_t events,lisaui_group_t* sender,void* user_data);



/**
 * @brief Initialize the LISAUI manager module
 *
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_init(void);

/**
 * @brief Deinitialize the LISAUI manager module
 *
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_deinit(void);

/**
 * @brief Add a callback function for specified manager events
 *
 * @param events Bitwise OR of LISAUI_MANAGER_EVENT_* values
 * @param cb Callback function pointer to be called when specified events occur
 * @param user_data User data pointer that will be passed to the callback function
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_add_callback(uint32_t events, lisaui_manager_event_cb_t cb, void* user_data);

/**
 * @brief Remove a previously registered callback function
 *
 * @param cb Callback function pointer to be removed
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_remove_callback(lisaui_manager_event_cb_t cb);

/**
 * @brief Register a group with the LISAUI manager
 * 
 * @param group Pointer to the group to be registered
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_register(lisaui_group_t *group);

/**
 * @brief Unregister a group from the LISAUI manager
 *
 * @param group Pointer to the group to be unregistered
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_unregister(lisaui_group_t *group);

/**
 * @brief Enter a specific group with the specified method and page index
 *
 * @param group_id ID of the group to enter
 * @param method Method to use when entering the group (pop stack top or use fixed page index)
 * @param page_index Index of the page to show (used when method is GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX)
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_enter(int group_id, lisaui_group_enter_page_method_t method, int page_index,uint32_t flags);

/**
 * @brief Exit a specific group
 *
 * @param group_id ID of the group to exit
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_exit(int group_id);


lisaui_group_t *lisaui_manager_get_current_group(void);

/**
 * @brief 向指定组ID或所有组发送事件
 *
 * @param group_id 目标组的ID，如果为LISAUI_GROUP_INDEX_RESERVED_ALL则发送给所有注册的组
 * @param events 要发送的事件，使用LISAUI_MANAGER_EVENT_*定义
 * @return lisaui_err_t LISAUI_ERR_OK成功，否则返回错误码
 */
lisaui_err_t lisaui_manager_send_event(lisaui_group_t *sender,uint32_t events);

/**
 * @brief 导航返回上一页或退出当前组
 *
 * 该函数实现UI的后退功能，关闭当前页面并返回上一个页面。
 * 若页面栈为空，则退出当前组。
 *
 * @return lisaui_err_t LISAUI_ERR_OK成功，否则返回错误码
 */
lisaui_err_t lisaui_manager_nav_back(void);

#ifdef __cplusplus
}
#endif

#endif // __LISAUI_MANAGER_H__
