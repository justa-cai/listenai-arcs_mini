#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lisa_log.h"
#include "lisa_err.h"
#include "lisa_mem.h"
// #include "evs_twdt.h"
#include "evs_event.h"
#include "lisa_time.h"
#include "sys_queue.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "lisa_semaphore.h"

/** Event TAG */
#define TAG "event"
/** Event Queue Max Count */
#define EVENT_QUEUE_COUNT_MAX (20)
/** Event Thread Stack Size */
#define EVENT_THREAD_STACK_SIZE (5 * 1024)
/** Event Thread Name */
#define EVENT_THREAD_NAME ("evs_event")
/** Event Queue Wait Time */
#define EVENT_LOOP_TIME (1000)
// WDT Feed Min time
#define EVENT_FEED_MIN_TIME (6 * 1000)
// WDT Feed Max time
#define EVENT_FEED_MAX_TIME (20 * 1000)

typedef struct evs_event_message_wrap_t {
	evs_event_message_t *msg;
	long delay;
	uint64_t time;
} evs_event_message_wrap_t;

typedef struct evs_event_message_node_t {
	evs_event_message_wrap_t *msg_wrap;
	SLIST_ENTRY(evs_event_message_node_t) next;
} evs_event_message_node_t;

typedef SLIST_HEAD(evs_event_message_nodes, evs_event_message_node_t) evs_event_message_nodes_t;

struct evs_event_s {
	lisa_queue_t *queue;
	lisa_thread_t *thread;
	lisa_semaphore_t *semaphore;
	lisa_mutex_t *mutex;
	evs_event_message_nodes_t msg_nodes;
	volatile bool is_wait;
};

static void _evs_event_run_task(void *param)
{
	evs_event_t *s_event = (evs_event_t *)param;
	evs_event_message_wrap_t msg_wrap;
	lisa_err_t pop_ret;

	while (true) {
		pop_ret = lisa_queue_pop(s_event->queue, &msg_wrap, sizeof(evs_event_message_wrap_t), EVENT_LOOP_TIME);
		// evs_twdt_add_and_feed(NULL, EVENT_FEED_MIN_TIME, EVENT_FEED_MAX_TIME);
		if (pop_ret != LISA_OK) continue;

		evs_event_message_wrap_t *cache_msg = (evs_event_message_wrap_t *)lisa_mem_alloc(sizeof(evs_event_message_wrap_t));
		if (!cache_msg) continue;

		memcpy(cache_msg, &msg_wrap, sizeof(evs_event_message_wrap_t));
		evs_event_message_node_t *msg_node = NULL, *last_node = NULL, *find_node = NULL;
		SLIST_FOREACH(msg_node, &s_event->msg_nodes, next)
		{
			if (msg_node->msg_wrap->time > msg_wrap.time) {
				find_node = last_node;
				break;
			}
			last_node = msg_node;
		}

		evs_event_message_node_t *cache_msg_node =
				(evs_event_message_node_t *)lisa_mem_calloc(1, sizeof(evs_event_message_node_t));
		if (cache_msg_node == NULL) {
			LISA_LOGE(TAG, "malloc cache_msg_node failed!");
			lisa_mem_free(cache_msg->msg);
			lisa_mem_free(cache_msg);
			continue;
		}
		cache_msg_node->msg_wrap = cache_msg;
		if (find_node != NULL) {
			SLIST_INSERT_AFTER(find_node, cache_msg_node, next);
		} else {
			if (last_node == NULL) {
				SLIST_INSERT_HEAD(&s_event->msg_nodes, cache_msg_node, next);
			} else {
				SLIST_INSERT_AFTER(last_node, cache_msg_node, next);
			}
		}

		//如果发现队列中有数据的话，取出，不然会进入事件处理列表中，导致队列中的消息没法按时处理
		while (!SLIST_EMPTY(&s_event->msg_nodes)) {
			lisa_mutex_lock(s_event->mutex, LISA_OS_WAIT_FOREVER);
			// 每次循环前检查队列中是否有数据，有此接口可不需要handler和runnable返回值的结果
			if (lisa_queue_waiting(s_event->queue) > 0) {
				// LISA_LOGD(TAG, "queue has more msg check in list %d", lisa_queue_waiting(s_event->queue));
				lisa_mutex_unlock(s_event->mutex);
				break;
			}
			evs_event_message_node_t *first_node = SLIST_FIRST(&s_event->msg_nodes);
			uint64_t cur_time = lisa_os_get_tick_ms();
			if (cur_time >= first_node->msg_wrap->time) {
				evs_event_message_t *msg = first_node->msg_wrap->msg;
				int ret = 0;
				if (msg->runnable != NULL) {
					LISA_LOGD(TAG, "evt msg -> enter, f: %p, d: %p", msg->runnable, msg->user_data);
					ret = msg->runnable(msg->user_data);
					LISA_LOGD(TAG, "evt msg -> exit");
				}
				SLIST_REMOVE(&s_event->msg_nodes, first_node, evs_event_message_node_t, next);
				lisa_mem_free(msg);
				lisa_mem_free(first_node->msg_wrap);
				lisa_mem_free(first_node);
				lisa_mutex_unlock(s_event->mutex);
			} else {
				uint32_t diff_time = first_node->msg_wrap->time - cur_time;
				s_event->is_wait = true;
				lisa_mutex_unlock(s_event->mutex);
				lisa_semaphore_take(s_event->semaphore, diff_time);
				lisa_mutex_lock(s_event->mutex, LISA_OS_WAIT_FOREVER);
				s_event->is_wait = false;
				cur_time = lisa_os_get_tick_ms();
				if (cur_time >= first_node->msg_wrap->time) {
					evs_event_message_t *msg = first_node->msg_wrap->msg;
					if (msg->runnable != NULL) {
						LISA_LOGD(TAG, "evt msg -> enter, f: %p, d: %p", msg->runnable, msg->user_data);
						msg->runnable(msg->user_data);
						LISA_LOGD(TAG, "evt msg -> exit");
					}
					SLIST_REMOVE(&s_event->msg_nodes, first_node, evs_event_message_node_t, next);
					lisa_mem_free(msg);
					lisa_mem_free(first_node->msg_wrap);
					lisa_mem_free(first_node);
					lisa_mutex_unlock(s_event->mutex);
				} else {
					//时间未到，被外部唤醒，重新从队列获取消息
					lisa_mutex_unlock(s_event->mutex);
					break;
				}
			}
		}
	}
}

evs_event_t *evs_event_create()
{
	evs_event_t *s_event = (evs_event_t *)lisa_mem_calloc(1, sizeof(evs_event_t));
	if (s_event == NULL) {
		LISA_LOGE(TAG, "malloc event failed!");
		return NULL;
	}
	s_event->queue = lisa_queue_create(EVENT_QUEUE_COUNT_MAX, (uint8_t *)"event", sizeof(evs_event_message_t));
	if (s_event->queue == NULL) {
		goto err_queue;
	}
	s_event->semaphore = lisa_semaphore_create(1);
	if (s_event->semaphore == NULL) {
		goto err_semaphore;
	}

	s_event->mutex = lisa_mutex_create();
	if (s_event->mutex == NULL) {
		goto err_mutex;
	}
	SLIST_INIT(&s_event->msg_nodes);
	lisa_thread_attr_t thread_attr;
	thread_attr.name = (uint8_t *)EVENT_THREAD_NAME;
	thread_attr.stack_size = EVENT_THREAD_STACK_SIZE;
	thread_attr.priority = LISA_OS_PRIORITY_NORMAL;
	s_event->thread = lisa_thread_create(&thread_attr, _evs_event_run_task, (void *)s_event);
	if (s_event->thread == NULL) {
		LISA_LOGE(TAG, "create thread error");
		goto err;
	}

	return s_event;
err:
	if (s_event->mutex != NULL) {
		lisa_mutex_delete(s_event->mutex);
	}

err_mutex:
	if (s_event->semaphore != NULL) {
		lisa_semaphore_delete(s_event->semaphore);
	}

err_semaphore:
	if (s_event->queue != NULL) {
		lisa_queue_delete(s_event->queue);
	}
err_queue:
	lisa_mem_free(s_event);
	return NULL;
}

int evs_event_send_msg_delay(evs_event_t *s_event, evs_event_message_t *msg, long delay)
{
	evs_event_message_wrap_t msg_wrap;
	evs_event_message_t *msg_copy =
			(evs_event_message_t *)lisa_mem_alloc(sizeof(evs_event_message_t));
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "malloc failed!");
		return -1;
	}
	memcpy(msg_copy, msg, sizeof(evs_event_message_t));
	msg_wrap.msg = msg_copy;
	msg_wrap.delay = delay;
	msg_wrap.time = lisa_os_get_tick_ms() + delay;
	// is_wait 添加volatile，同时去除锁，防止消息处理中，耽搁外部任务消息
	if (s_event->is_wait) {
		// LISA_LOGI(TAG, "event is wait msg, wakeup");
		lisa_semaphore_give(s_event->semaphore);
	}

	if (lisa_queue_push(s_event->queue, &msg_wrap, sizeof(evs_event_message_wrap_t), 0) !=
			LISA_OK) {
		LISA_LOGE(TAG, "send empty msg error");
		lisa_mem_free(msg_copy);
		return -1;
	}

	return 0;
}

int evs_event_post_runnable(evs_event_t *s_event, evs_event_runnable runnable, void *user_data)
{
	return evs_event_post_runnable_delay(s_event, runnable, user_data, 0);
}

int evs_event_post_runnable_delay(
		evs_event_t *s_event, evs_event_runnable runnable, void *user_data, long delay)
{
	evs_event_message_t msg = {0, 0, 0, runnable, user_data};
	return evs_event_send_msg_delay(s_event, &msg, delay);
}

void evs_event_destroy(evs_event_t *s_event)
{
	lisa_mutex_lock(s_event->mutex, LISA_OS_WAIT_FOREVER);
	if (s_event->is_wait) {
		lisa_semaphore_give(s_event->semaphore);
	}
	lisa_mutex_unlock(s_event->mutex);

	lisa_mutex_lock(s_event->mutex, LISA_OS_WAIT_FOREVER);
	if (s_event->thread != NULL) {
		lisa_thread_delete(s_event->thread);
	}
	evs_event_message_node_t *msg_node, *tmp_node;
	SLIST_FOREACH_SAFE(msg_node, &s_event->msg_nodes, next, tmp_node)
	{
		SLIST_REMOVE(&(s_event->msg_nodes), msg_node, evs_event_message_node_t, next);
		lisa_mem_free(msg_node->msg_wrap);
		lisa_mem_free(msg_node);
	}
	evs_event_message_wrap_t msg_wrap;
	while (lisa_queue_pop(s_event->queue, &msg_wrap, sizeof(evs_event_message_wrap_t), 0) ==
			LISA_OK) {
		lisa_mem_free(msg_wrap.msg);
	}
	lisa_queue_delete(s_event->queue);
	lisa_mutex_unlock(s_event->mutex);

	if (s_event->mutex != NULL) {
		lisa_mutex_delete(s_event->mutex);
	}
	if (s_event->semaphore != NULL) {
		lisa_semaphore_delete(s_event->semaphore);
	}
	lisa_mem_free(s_event);
}
