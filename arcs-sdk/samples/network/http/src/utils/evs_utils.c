#include <stdlib.h>
#include "evs_utils.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include <string.h>

#define TAG "evs_utils"

static evs_event_t *g_event = NULL;

void evs_utils_init()
{
	if (g_event == NULL) {
		g_event = evs_event_create();
		if (g_event == NULL) {
			LISA_LOGE(TAG, "init handler error");
			return;
		}
	}
}

int evs_handler_post_runnable(evs_event_runnable runnable, void *user_data)
{
	if (g_event != NULL) {
		return evs_event_post_runnable(g_event, runnable, user_data);
	}
	return -1;
}

int evs_handler_post_runnable_delay(evs_event_runnable runnable, void *user_data, long delay)
{
	if (g_event != NULL) {
		return evs_event_post_runnable_delay(g_event, runnable, user_data, delay);
	}
	return -1;
}

void evs_utils_uninit()
{
	if (g_event != NULL) {
		evs_event_destroy(g_event);
		g_event = NULL;
	}
}
