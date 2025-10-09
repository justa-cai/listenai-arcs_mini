#include <stdlib.h>
#include "utils/evs_utils.h"
#include "lisa_mem.h"
#include "lisa_log.h"

#define CONFIG_EVS_MEMP (1)  //是否使用内存池
#if CONFIG_EVS_MEMP
#include "memorypool.h"
#endif

#define TAG "evs_utils"

static evs_event_t *g_event = NULL;

#if CONFIG_EVS_MEMP

typedef struct evs_memp_desc_t {
	MemoryPool *memp;
	evs_memp_type_e type;
	int num;
	int size;
	char *desc;
} evs_memp_desc_t;

#define EVS_MEMP_DECLARE(name, num, size, desc) \
	static evs_memp_desc_t memp_##name = {NULL, name, num, size, desc};

EVS_MEMP_DECLARE(MEMP_TYPE_DEFAULT, 10000, sizeof(char), "MEMP_DEFAULT")
EVS_MEMP_DECLARE(MEMP_TYPE_SOCKET, 5000, sizeof(char), "MEMP_SOCKET")

#undef EVS_MEMP_DECLARE

static evs_memp_desc_t *evs_mem_pools[MEMP_TYPE_MAX] = {
#define EVS_MEMP_DECLARE(name) &memp_##name
		EVS_MEMP_DECLARE(MEMP_TYPE_DEFAULT),
		EVS_MEMP_DECLARE(MEMP_TYPE_SOCKET),
#undef EVS_MEMP_DECLARE
};

#endif

void evs_utils_init()
{
#if CONFIG_EVS_MEMP
	for (int i = 0; i < MEMP_TYPE_MAX; i++) {
		evs_memp_desc_t *memp_desc = evs_mem_pools[i];
		if (memp_desc->memp == NULL) {
			int pool_size = memp_desc->num * memp_desc->size;
			memp_desc->memp = MemoryPoolInit(pool_size * 2, pool_size);
		}
	}
#endif
	if (g_event == NULL) {
		g_event = evs_event_create();
		if (g_event == NULL) {
			LISA_LOGE(TAG, "init handler error");
			return;
		}
	}
}

/* Handler消息接口 */
void
evs_handler_set_handler(evs_event_handler handler)
{
	if (g_event != NULL) {
		evs_event_set_handler(g_event, handler);
	}
}

void
evs_handler_send_empty_msg(int what)
{
	if (g_event != NULL) {
		evs_event_send_empty_msg(g_event, what);
	}
}

void
evs_handler_send_empty_msg_delay(int what, long delay)
{
	if (g_event != NULL) {
		evs_event_send_empty_msg_delay(g_event, what, delay);
	}
}

void
evs_handler_send_msg(evs_event_message_t *msg)
{
	if (g_event != NULL) {
		evs_event_send_msg(g_event, msg);
	}
}

void
evs_handler_send_msg_delay(evs_event_message_t *msg, long delay)
{
	if (g_event != NULL) {
		evs_event_send_msg_delay(g_event, msg, delay);
	}
}

void
evs_handler_post_runnable(evs_event_runnable runnable, void *user_data)
{
	if (g_event != NULL) {
		evs_event_post_runnable(g_event, runnable, user_data);
	}
}

void
evs_handler_post_runnable_delay(evs_event_runnable runnable, void *user_data, long delay)
{
	if (g_event != NULL) {
		evs_event_post_runnable_delay(g_event, runnable, user_data, delay);
	}
}

/* 内存池接口 */
void *
evs_memp_malloc(unsigned long size)
{
	return evs_memp_malloc_from(MEMP_TYPE_DEFAULT, size);
}

void *
evs_memp_malloc_set(unsigned long size, int c)
{
	return evs_memp_malloc_set_from(MEMP_TYPE_DEFAULT, size, c);
}

void *
evs_memp_malloc_from(evs_memp_type_e type, unsigned long size)
{
#if CONFIG_EVS_MEMP
	MemoryPool *memp = evs_mem_pools[type]->memp;
	if (memp != NULL) {
		void *mem = MemoryPoolAlloc(memp, size);
        
        LISA_APP_ASSERT(mem, "MemoryPoolAlloc size:%ld failed",size);

		return mem;
	}
	return NULL;
#else
	void *mem = lisa_mem_alloc(size);
    LISA_APP_ASSERT(mem, "lisa_mem_alloc size:%ld failed",size);    
    return mem;
#endif
}

void *
evs_memp_malloc_set_from(evs_memp_type_e type, unsigned long size, int c)
{
	void *mem = evs_memp_malloc_from(type, size);
	if (mem != NULL) {
		memset(mem, c, size);
	}
	return mem;
}

void
evs_memp_free(void *mem)
{
	return evs_memp_free_from(MEMP_TYPE_DEFAULT, mem);
}

void
evs_memp_free_from(evs_memp_type_e type, void *mem)
{
#if CONFIG_EVS_MEMP
	MemoryPool *memp = evs_mem_pools[type]->memp;
	if (memp != NULL && mem != NULL) {
		MemoryPoolFree(memp, mem);
	}
#else
	if (mem != NULL) {
		lisa_mem_free(mem);
	}
#endif
    else
    {
        LISA_LOGE(TAG, "evs_memp_free_from failed,because mem is null"); 
    }
}

void
evs_memp_dump()
{
#if CONFIG_EVS_MEMP
	for (int i = 0; i < MEMP_TYPE_MAX; i++) {
		MemoryPool *memp = evs_mem_pools[i]->memp;
		if (memp != NULL) {
			// LISA_LOGI(TAG, "mem_pools desc: %s", evs_mem_pools[i]->desc);
			// LISA_LOGI(TAG, "->> Memory Usage: %.4lf", MemoryPoolGetUsage(memp));
			// LISA_LOGI(TAG, "->> Memory Usage(prog): %.4lf", MemoryPoolGetProgUsage(memp));
			// LISA_LOGI(TAG, "mem_pools desc: %s", evs_mem_pools[i]->desc);
		}
	}
#else
#endif
}

void *
evs_malloc(unsigned int size)
{
	return lisa_mem_alloc(size);
}

void *
evs_calloc(unsigned int num, unsigned int size)
{
	void *ptr = lisa_mem_alloc(num * size);
	if (ptr != NULL) {
		memset(ptr, 0, num * size);
	}
	return ptr;
}

void
evs_free(void *ptr)
{
	return lisa_mem_free(ptr);
}

bool
evs_is_power_on()
{
	return true;
}

static char
dec2hex(short int c)
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


void evs_utils_uninit()
{
	if (g_event != NULL) {
		evs_event_destroy(g_event);
		g_event = NULL;
	}
#if CONFIG_EVS_MEMP
	for (int i = 0; i < MEMP_TYPE_MAX; i++) {
		evs_memp_desc_t *memp_desc = evs_mem_pools[i];
		if (memp_desc->memp != NULL) {
			MemoryPoolDestroy(memp_desc->memp);
			memp_desc->memp = NULL;
		}
	}
#endif
}
