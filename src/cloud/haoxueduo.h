#ifndef __HAOXUEDUO_H__
#define __HAOXUEDUO_H__

#include "stdint.h"
struct haoxueduo_role_info {
    char *desc;
    char *voice_model;
    char *start_text;
};

struct haoxueduo_role {
    uint32_t id;
    char *name;
    char *icon_url;
    char *token;
    struct haoxueduo_role_info info;
};

/**
 * 获取角色列表
 */
int haoxueduo_roles_get(struct haoxueduo_role **roles, uint32_t *cnt);

/**
 * 开始对话
 */
int haoxueduo_chat_start(const char *name);

/**
 * 停止对话
 */
int haoxueduo_chat_stop();

/**
 * 从服务器获取角色列表回调函数
 *
 * @param state 状态 非0失败, 0成功
 * @param roles 角色列表
 * @param cnt 角色数量
 * @param user_data 用户数据
 */
typedef void (*haoxueduo_roles_info_request_cb_t)(int state, const struct haoxueduo_role *roles, uint32_t cnt,
                                                  void *user_data);

/**
 * 从服务器获取角色列表
 */
void haoxueduo_roles_info_request(haoxueduo_roles_info_request_cb_t cb, void *user_data);

/*
*检查是否需要获取角色列表
*/
int haoxueduo_check_for_get_roles(void);

#endif