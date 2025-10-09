#ifndef FUNC_PROXY_H
#define FUNC_PROXY_H

#include <stdint.h>
#include <stddef.h>
#include "irq_proxy.h"

// 使用irq_proxy.h中定义的通道
#define IRQ_PROXY_CHAN_FUNC_PROXY     IRQ_PROXY_CHAN_FUNC_REQUEST

/**
 * 函数代理数据结构
 */
typedef struct {
    void *data;          // 数据指针
    size_t size;         // 数据大小
} func_proxy_data_t;

/**
 * 函数代理处理函数
 * @param in: 输入数据
 * @param out: 输出数据
 * @return: 0成功，其他值失败
 */
typedef int32_t (*func_proxy_handler_t)(func_proxy_data_t *in, func_proxy_data_t *out);

/**
 * AP核注册函数处理回调
 * @param name: 函数名称，必须是全局唯一的
 * @param handler: 函数处理回调
 * @return: 0成功，其他值失败
 */
int32_t func_proxy_register(const char *name, func_proxy_handler_t handler);

/**
 * CP核调用AP核函数
 * @param name: 函数名称
 * @param in: 输入数据
 * @param out: 输出数据
 * @return: 0成功，其他值失败
 */
int32_t func_proxy_call(const char *name, func_proxy_data_t *in, func_proxy_data_t *out);

/**
 * 初始化函数代理
 */
void func_proxy_init(void);

#endif /* FUNC_PROXY_H */
