#ifndef SERVICE_ALARM_H
#define SERVICE_ALARM_H

#include <stdint.h>
#include <stddef.h>
#include "alarm_store.h"

struct service_alarm {
    uint64_t timestamp;
    uint8_t text[128];
};

int service_alarm_create_obj(const alarm_object_t *alarm, char *err_msg, size_t err_len);
int service_alarm_delete_by_cloud_id(uint64_t cloud_id, char *err_msg, size_t err_len);
const alarm_object_t *service_alarm_find_by_cloud_id(uint64_t cloud_id);

void service_alarm_init(void);
int service_alarm_add(uint64_t timestamp, const uint8_t *text);
int service_alarm_delete(uint64_t timestamp);
int service_alarm_get_count(void);
int service_alarm_delete_all(uint32_t cnt);
uint8_t service_alarm_init_done(void);
struct service_alarm *service_alarm_get_all(uint32_t *cnt);

#endif
