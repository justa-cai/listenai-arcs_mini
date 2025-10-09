#ifndef LISA_EVS_UTILS_H
#define LISA_EVS_UTILS_H
#include <stdbool.h>
#include <stdint.h>

#include "evs_event.h"

typedef enum {
	MEMP_TYPE_DEFAULT = 0,
	MEMP_TYPE_SOCKET,
	MEMP_TYPE_MAX  //不使用，计数用的
} evs_memp_type_e;

void evs_utils_init();

/* Handler消息接口 */
void evs_handler_set_handler(evs_event_handler handler);
void evs_handler_send_empty_msg(int what);
void evs_handler_send_empty_msg_delay(int what, long delay);
void evs_handler_send_msg(evs_event_message_t *msg);  // msg 和 user_data需要自己回收
void evs_handler_send_msg_delay(evs_event_message_t *msg, long delay);
void evs_handler_post_runnable(evs_event_runnable runnable, void *user_data);
void evs_handler_post_runnable_delay(evs_event_runnable runnable, void *user_data, long delay);

/* 内存池接口 */
void *evs_memp_malloc(unsigned long size);
void *evs_memp_malloc_set(unsigned long size, int c);
void *evs_memp_malloc_from(evs_memp_type_e type, unsigned long size);
void *evs_memp_malloc_set_from(evs_memp_type_e type, unsigned long size, int c);
void evs_memp_free_from(evs_memp_type_e type, void *mem);
void evs_memp_free(void *mem);
void evs_memp_dump();

/* 内存  */
void *evs_malloc(unsigned int size);
void *evs_calloc(unsigned int num, unsigned int size);
void evs_free(void *ptr);

/* 是否是上电开机 */
bool evs_is_power_on();

void evs_utils_uninit();

/* urlencode */
int lisa_evs_urlencode(const uint8_t *in, int inlen, uint8_t *out, int outlen);

#endif /* LISA_EVS_UTILS_H */
