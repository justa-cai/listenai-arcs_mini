/**
 * @file lisa_evt_pub.h
 * @brief LISA 事件发布器 API
 *
 * 此文件提供线程安全的LISA 事件发布器接口，实现观察者模式的发布-订阅机制，
 * 支持事件的发布、订阅和回调处理，为组件间通信提供解耦的事件驱动架构。
 */

#ifndef __LISA_EVT_PUBLISHER_H__
#define __LISA_EVT_PUBLISHER_H__

#include <stdint.h>
#include <stddef.h>

/**
 * @brief 事件发布器句柄
 *
 * 表示事件发布器实例的句柄类型，用于标识一个事件发布器。
 */
typedef void *lisa_evt_publisher_t;

/**
 * @brief 事件发布器回调函数类型
 *
 * 事件订阅者的回调函数类型定义，当发布的事件被订阅时，将调用此回调函数。
 *
 * @param evt 事件ID，用于标识具体的事件类型
 * @param data 事件数据指针，可能为 NULL
 * @param data_len 事件数据长度（字节），当 data 为 NULL 时此值为 0
 * @param user_data 用户自定义数据，在订阅时传入
 */
typedef void (*lisa_evt_publisher_cb_t)(uint32_t evt, void *data, uint32_t data_len,
				       void *user_data);

/**
 * @brief 创建新的事件发布器
 *
 * 创建一个新的事件发布器实例，用于事件的发布和订阅管理。
 * 事件按照BIT位进行管理
 *
 * @return 事件发布器句柄，失败时返回 NULL
 */
lisa_evt_publisher_t lisa_evt_publisher_new();

/**
 * @brief 创建基于事件队列的发布器
 *
 * 创建一个使用事件队列的新事件发布器实例。
 * 与 `lisa_evt_publisher_new()` 不同，此函数创建的发布器按照订阅事件的数值进行匹配发布，
 *
 *
 * @return 事件发布器句柄，失败时返回 NULL
 */
lisa_evt_publisher_t lisa_evt_publisher_new_eq();

/**
 * @brief 发布事件
 *
 * 向指定的发布器发布一个事件，所有订阅了该事件的订阅者都会收到通知。
 *
 * @param p 事件发布器句柄
 * @param evt 事件ID，用于标识具体的事件类型
 * @param data 事件数据指针，可以为 NULL
 * @param len 事件数据长度（字节），当 data 为 NULL 时此值应为 0
 */
void lisa_evt_publisher_publish(lisa_evt_publisher_t p, uint32_t evt, void *data, uint32_t len);

/**
 * @brief 添加事件订阅
 *
 * 为指定的事件ID添加订阅者，当该事件被发布时，回调函数将被调用。
 *
 * @param p 事件发布器句柄
 * @param evt 要订阅的事件ID
 * @param cb 事件回调函数指针
 * @param user_data 用户自定义数据，将在回调时传递给回调函数
 * @return 0 表示成功，非 0 表示失败
 */
int lisa_evt_publisher_evt_add(lisa_evt_publisher_t p, uint32_t evt, lisa_evt_publisher_cb_t cb,
			       void *user_data);

/**
 * @brief 移除回调订阅
 *
 * 移除指定的回调函数订阅，该回调函数将不再接收任何事件通知。
 *
 * @param p 事件发布器句柄
 * @param cb 要移除的回调函数指针
 */
void lisa_evt_publisher_cb_remove(lisa_evt_publisher_t p, lisa_evt_publisher_cb_t cb);

/**
 * @brief 清空所有订阅
 *
 * 清空事件发布器中的所有事件订阅，移除所有订阅者。
 *
 * @param p 事件发布器句柄
 * @return 0 表示成功，非 0 表示失败
 */
int lisa_evt_publisher_clear(lisa_evt_publisher_t p);

/**
 * @brief 销毁事件发布器
 *
 * 销毁指定的事件发布器，释放所有相关资源。
 * 销毁后会自动清理所有的事件订阅。
 *
 * @param p 事件发布器句柄
 */
void lisa_evt_publisher_destroy(lisa_evt_publisher_t p);

/**
 * @brief 发布无数据事件
 *
 * 便利函数，用于发布不携带任何数据的事件。
 * 相当于调用 `lisa_evt_publisher_publish(p, evt, NULL, 0)`。
 *
 * @param p 事件发布器句柄
 * @param evt 事件ID，用于标识具体的事件类型
 */
static inline void lisa_evt_publisher_publish_without_data(lisa_evt_publisher_t p, uint32_t evt)
{
	lisa_evt_publisher_publish(p, evt, NULL, 0);
}

/**
 * @brief 检查事件是否有订阅者
 *
 * 检查指定的事件ID是否已被订阅，即是否有订阅者会接收该事件的通知。
 *
 * @param p 事件发布器句柄
 * @param evt 要检查的事件ID
 * @return 1 表示有订阅者，0 表示无订阅者
 */
int lisa_evt_publisher_has_subscriber(lisa_evt_publisher_t p, uint32_t evt);
#endif
