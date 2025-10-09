#ifndef __LISTEN_UTILS_H__
#define __LISTEN_UTILS_H__
#include <stdbool.h>
#include <stdint.h>

#include "evs_event.h"

void evs_utils_init();

int evs_handler_post_runnable(evs_event_runnable runnable, void *user_data);
int evs_handler_post_runnable_delay(evs_event_runnable runnable, void *user_data, long delay);

void evs_utils_uninit();

#endif
