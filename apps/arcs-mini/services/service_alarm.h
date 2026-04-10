#ifndef SERVICE_ALARM_H
#define SERVICE_ALARM_H

#include <stdint.h>
#include <stddef.h>
#include "alarm_constructor.h"
#include "alarm_nvs.h"

struct service_alarm {
    uint64_t timestamp;
    uint8_t text[LS_ALARM_TEXT_MAX_LEN];
};

void service_alarm_event_init(struct service_alarm *alarm,
                              uint64_t timestamp,
                              const uint8_t *text);

/* ===========================================================================*/
/* 闹钟操作接口                                                                */
/* ============================================================================*/

// 核心接口：接收完整闹钟对象
int service_alarm_create(const alarm_object_t *alarm, char *err_msg, size_t err_len);
int service_alarm_delete_by_cloud_id(uint64_t cloud_id, char *err_msg, size_t err_len);
const alarm_object_t *service_alarm_find_by_cloud_id(uint64_t cloud_id);

// 便捷接口：从参数构造并创建（给 MCP 用）
int service_alarm_create_from_params(const alarm_construct_params_t *params, char *err_msg, size_t err_len);


/* ===========================================================================*/
/* 闹钟初始化接口                                                              */
/* ============================================================================*/

// 初始化闹钟服务(加载 NVS 数据到链表)
void service_alarm_init(void);

// 反初始化闹钟服务(清空所有闹钟数据和初始化标志)
void service_alarm_deinit(void);

// 获取闹钟初始化完成标志
uint8_t service_alarm_init_done(void);

#endif
