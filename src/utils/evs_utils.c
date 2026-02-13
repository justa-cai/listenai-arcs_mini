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

static char dec2hex(short int c)
{
	if (0 <= c && c <= 9) {
		return c + '0';
	} else if (10 <= c && c <= 15) {
		return c + 'A' - 10;
	} else {
		return -1;
	}
}

int lisa_evs_urlencode(const uint8_t *in, int inlen, uint8_t *out, int outlen)
{
	int i = 0;
	int res_len = 0;
	for (i = 0; i < inlen; ++i) {
		uint8_t c = *in;
		in++;
		if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
				c == '/' || c == '.') {
			out[res_len++] = c;
		} else {
			int j = (short int)c;
			if (j < 0) j += 256;
			int i1, i0;
			i1 = j / 16;
			i0 = j - i1 * 16;
			out[res_len++] = '%';
			out[res_len++] = dec2hex(i1);
			out[res_len++] = dec2hex(i0);
		}
	}
	out[res_len] = '\0';
	int outlen2 = strlen(out);

	return outlen2;
}
