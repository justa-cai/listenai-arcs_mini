#ifndef __ALARM_NVS_H__
#define __ALARM_NVS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "alarm_types.h"

/* 闹钟列表（存储所有 cloud_id，用于遍历查询）*/
typedef struct {
    uint64_t cloud_id;
} alarm_list_item_t;

#define ALARM_LIST_KEY "user.alarm_list"

/* NVS 对象操作接口 */
int alarm_nvs_init(void);
int alarm_nvs_add(const alarm_object_t *alarm);
int alarm_nvs_delete(uint64_t cloud_id);
int alarm_nvs_update(const alarm_object_t *alarm);

/* 查询接口 */
const alarm_object_t *alarm_nvs_find(uint64_t cloud_id);
int alarm_nvs_count(void);
int alarm_nvs_get_all(alarm_object_t *alarms, int max_count);

/* 工具接口 */
void alarm_obj_print(const alarm_object_t *alarm);

#ifdef __cplusplus
}
#endif

#endif // __ALARM_NVS_H__