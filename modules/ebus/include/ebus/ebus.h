/**
 * @file ebus.h
 * @brief 软件总线通讯框架头文件
 *
 * ebus是一个轻量级的软件总线通讯框架，用于组件间的消息传递。
 * 它实现了发布-订阅模式，支持同步消息处理。
 */

#ifndef __EBUS_H__
#define __EBUS_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 错误码定义
 */
#define EBUS_OK          0       /**< 操作成功 */
#define EBUS_ERROR       -1      /**< 一般错误 */

#define EBUS_EVENT_ALL             (0)
#define EBUS_EVENT_CODE_RESERVED   (5)
/**
 * @brief 总线句柄结构体（不透明类型）
 */
typedef struct ebus_handle ebus_handle_t;

/**
 * @brief 通道结构体（不透明类型）
 */
typedef struct ebus_chn ebus_chn_t;

/**
 * @brief 订阅类型枚举
 */
typedef enum {
    EBUS_SUBSCRIBER_TYPE_SYNC,   /**< 同步订阅 */
    EBUS_SUBSCRIBER_TYPE_ASYNC, /**< 异步订阅（暂未实现）*/
} ebus_subscribe_type_e;

/**
 * @brief 通道回调函数类型定义
 * 
 * @param chn 通道句柄
 * @param message 消息数据
 * @param msg_size 消息大小
 * @param user_data 用户数据
 * @return int 回调处理结果，0表示成功
 */
typedef int (*ebus_chn_cb_t)(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size, void *user_data);

/**
 * @brief 初始化ebus框架
 * 
 * 在使用ebus框架前必须先调用此函数进行初始化。
 * 
 * @return int 0表示成功，负值表示错误码
 */
int ebus_init(void);

/**
 * @brief 创建一个总线
 * 
 * @param bus_name 总线名称，不能为NULL，长度不超过EBUS_NAME_MAX_LEN
 * @return ebus_handle_t* 成功返回总线句柄，失败返回NULL
 */
ebus_handle_t *ebus_create(const char *bus_name);

/**
 * @brief 销毁总线
 * 
 * 释放总线相关的所有资源。
 * 
 * @param bus 总线句柄，不能为NULL
 * @return int 0表示成功，负值表示错误码
 */
int ebus_destroy(ebus_handle_t *bus);

/**
 * @brief 创建并附加通道到总线
 * 
 * @param bus 总线句柄，不能为NULL
 * @param chn_name 通道名称，不能为NULL，长度不超过EBUS_CHN_NAME_MAX_LEN
 * @return ebus_chn_t* 成功返回通道句柄，失败返回NULL
 */
ebus_chn_t *ebus_chn_create_attach(ebus_handle_t *bus, const char *chn_name);

/**
 * @brief 绑定到已存在的通道
 * 
 * 此函数会阻塞等待直到找到指定的通道。
 * 
 * @param bus_name 总线名称，不能为NULL
 * @param chn_name 通道名称，不能为NULL
 * @return ebus_chn_t* 成功返回通道句柄，失败返回NULL
 */
ebus_chn_t *ebus_chn_bind(const char *bus_name, const char *chn_name);

/**
 * @brief 订阅通道消息
 * 
 * @param chn 通道句柄，不能为NULL
 * @param code 消息代码
 * @param type 订阅类型，目前仅支持EBUS_SUBSCRIBER_TYPE_SYNC
 * @param code 消息代码
 * @param cb 回调函数，不能为NULL
 * @param user_data 用户数据，将传递给回调函数
 * @return int 0表示成功，负值表示错误码
 */
int ebus_message_subscribe(ebus_chn_t *chn, ebus_subscribe_type_e type, uint32_t code, ebus_chn_cb_t cb, void *user_data);

/**
 * @brief 取消订阅
 * 
 * @param chn 通道句柄，不能为NULL
 * @param cb 回调函数，不能为NULL
 * @return int 
 */
int ebus_message_unsubscribe(ebus_chn_t *chn, ebus_chn_cb_t cb);

/**
 * @brief 发布消息到通道
 *
 * 此函数会遍历所有订阅者并同步调用其回调函数。
 *
 * @param chn 通道句柄，不能为NULL
 * @param code 消息代码
 * @param message 消息数据
 * @param msg_size 消息大小
 * @return int 0表示成功，负值表示错误码
 */
int ebus_message_pub(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size);
int ebus_message_pub_async(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size);

#ifdef __cplusplus
}
#endif

#endif /* __EBUS_H__ */