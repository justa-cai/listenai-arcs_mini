#ifndef __ALARM_RING_H__
#define __ALARM_RING_H__

#include <stdint.h>
#include <stdbool.h>
#include "sound_player.h"

#ifdef __cplusplus
extern "C" {
#endif

void alarm_ring_init(sound_player_t *sound_player);
void alarm_ring_on_alarm(uint64_t timestamp, const uint8_t *text);
void alarm_ring_on_tts_url(const char *url);
void alarm_ring_stop(void);
bool alarm_ring_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
