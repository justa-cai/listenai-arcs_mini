#ifndef __ALARM_RING_H__
#define __ALARM_RING_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*alarm_ring_play_once_cb_t)(const char *text);
typedef void (*alarm_ring_force_stop_cb_t)(void);

int alarm_ring_init(alarm_ring_play_once_cb_t play_cb, alarm_ring_force_stop_cb_t stop_cb);
int alarm_ring_start(const char *text);
void alarm_ring_stop(void);
void alarm_ring_notify_playback_complete(void);
bool alarm_ring_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
