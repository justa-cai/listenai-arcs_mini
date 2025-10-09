#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/evs_event.h"
#include "utils/evs_utils.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "lisa_err.h"
#include "lisa_time.h"
#include "lisa_typedef.h"
#include "utils/queue.h"

#define TAG "evs_event"
#define MAX_QUEUE_COUNT (20)
#define THREAD_MAIN_STACK_SIZE (5 * 1024)

#define OS_MSEC_PER_SEC (1000U) /* milliseconds per second */
#define OS_USEC_PER_MSEC (1000U) /* microseconds per millisecond */
#define OS_USEC_PER_SEC (1000000U) /* microseconds per second */

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
	evs_event_handler handler;
	volatile bool is_wait;
};

static void _evs_event_run_task(void *param)
{
	evs_event_t *s_event = (evs_event_t *)param;
	evs_event_message_wrap_t msg_wrap;
	memset(&msg_wrap, 0, sizeof(evs_event_message_wrap_t));

	while (true) {
		if (lisa_queue_pop(s_event->queue, &msg_wrap, sizeof(evs_event_message_wrap_t),
					LISA_OS_WAIT_FOREVER) == LISA_OK) {
			LISA_LOGD(TAG, "evs_event_run_task ptr: %p", msg_wrap.msg->user_data);
			evs_event_message_wrap_t *cache_msg =
					(evs_event_message_wrap_t *)evs_memp_malloc(sizeof(evs_event_message_wrap_t));
			if (cache_msg == NULL) {
				LISA_LOGE(TAG, "malloc cache_msg failed!");
				continue;
			}
			memcpy(cache_msg, &msg_wrap, sizeof(evs_event_message_wrap_t));
			// LISA_LOGD(TAG, "evs_event_run_task cache_msg user_data ptr: %p",
			// cache_msg->msg->user_data);
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
					(evs_event_message_node_t *)evs_memp_malloc_set(
							sizeof(evs_event_message_node_t), 0);
			if (cache_msg_node == NULL) {
				LISA_LOGE(TAG, "malloc cache_msg_node failed!");
				evs_memp_free(cache_msg->msg);
				evs_memp_free(cache_msg);
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
			// if (uxQueueMessagesWaiting(event->queue.handle) > 0) {
			//     LISA_LOGI(TAG, "queue has more msg");
			//     continue;
			// }
			while (!SLIST_EMPTY(&s_event->msg_nodes)) {
				LISA_LOGI(TAG, "s_event->mutex wait");
				lisa_mutex_lock(s_event->mutex, LISA_OS_WAIT_FOREVER);
				// 每次循环前检查队列中是否有数据，有此接口可不需要handler和runnable返回值的结果
				if (lisa_queue_waiting(s_event->queue) > 0) {
					LISA_LOGI(TAG, "queue has more msg check in list");
					lisa_mutex_unlock(s_event->mutex);
					break;
				}
				evs_event_message_node_t *first_node = SLIST_FIRST(&s_event->msg_nodes);
				// uint32_t cur_time = lisa_os_get_time() * OS_MSEC_PER_SEC;
				uint64_t cur_time = lisa_os_get_ticks();
				if (cur_time >= first_node->msg_wrap->time) {
					evs_event_message_t *msg = first_node->msg_wrap->msg;
					int ret = 0;
					if (msg->runnable != NULL) {
						// LISA_LOGD(TAG, "evs_event_run_task msg->runnable user_data ptr: %p",
						// msg->user_data);
						ret = msg->runnable(msg->user_data);
					} else {
						if (s_event->handler != NULL) {
							ret = s_event->handler(msg);
						}
					}
					SLIST_REMOVE(&s_event->msg_nodes, first_node, evs_event_message_node_t, next);
					evs_memp_free(msg);
					evs_memp_free(first_node->msg_wrap);
					evs_memp_free(first_node);
					// 如果有回调中消息嵌套的话，需要返回EVS_EVENT_NEST,防止没法处理消息队列中的数据
					if (ret == EVS_EVENT_NEST) {
						lisa_mutex_unlock(s_event->mutex);
						break;
					}
					lisa_mutex_unlock(s_event->mutex);
				} else {
					uint32_t diff_time = first_node->msg_wrap->time - cur_time;
					s_event->is_wait = true;
					lisa_mutex_unlock(s_event->mutex);
					lisa_semaphore_take(s_event->semaphore, diff_time);
					LISA_LOGI(TAG, "s_event->mutex wait else");
					lisa_mutex_lock(s_event->mutex, LISA_OS_WAIT_FOREVER);
					s_event->is_wait = false;
					// cur_time = lisa_os_get_time() * OS_MSEC_PER_SEC;
					cur_time = lisa_os_get_ticks();
					if (cur_time >= first_node->msg_wrap->time) {
						evs_event_message_t *msg = first_node->msg_wrap->msg;
						int ret = 0;
						if (msg->runnable != NULL) {
							ret = msg->runnable(msg->user_data);
						} else {
							if (s_event->handler != NULL) {
								ret = s_event->handler(msg);
							}
						}
						SLIST_REMOVE(
								&s_event->msg_nodes, first_node, evs_event_message_node_t, next);
						evs_memp_free(msg);
						evs_memp_free(first_node->msg_wrap);
						evs_memp_free(first_node);
						//如果有回调中消息嵌套的话，需要返回EVS_EVENT_NEST,防止没法处理消息队列中的数据
						if (ret == EVS_EVENT_NEST) {
							lisa_mutex_unlock(s_event->mutex);
							break;
						}
						lisa_mutex_unlock(s_event->mutex);
					} else {
						//时间未到，被外部唤醒，重新从队列获取消息
						lisa_mutex_unlock(s_event->mutex);
						break;
					}
				}
			}
			LISA_LOGI(TAG, "s_event->queue wait");
			// list_memheap();
			// list_msgqueue();
		} else {
			LISA_LOGE(TAG, "WAIT QUEUE ERROR");
		}
	}
	LISA_LOGE(TAG, "_evs_event_run_task  OUT");
}

evs_event_t *evs_event_create()
{
	evs_event_t *s_event = (evs_event_t *)evs_calloc(1, sizeof(evs_event_t));
	if (s_event == NULL) {
		LISA_LOGE(TAG, "malloc event failed!");
		return NULL;
	}
	s_event->queue = lisa_queue_create(MAX_QUEUE_COUNT, "evs_event", sizeof(evs_event_message_t));
	if (s_event->queue == NULL) {
		goto err_queue;
	}
	s_event->semaphore = lisa_semaphore_create(1);
	if (s_event->semaphore == NULL) {
		goto err_semaphore;
	}
	/* clear semaphore */
	lisa_semaphore_take(s_event->semaphore, 0);

	s_event->mutex = lisa_mutex_create();
	if (s_event->mutex == NULL) {
		goto err_mutex;
	}
	SLIST_INIT(&s_event->msg_nodes);
	lisa_thread_attr_t thread_attr;
	thread_attr.name = TAG;
	thread_attr.stack_size = THREAD_MAIN_STACK_SIZE;
	// TODO
	thread_attr.priority = LISA_OS_PRIORITY_ABOVE_NORMAL; // OS_PRIORITY_ABOVE_NORMAL
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
	evs_free(s_event);
	return NULL;
}

void evs_event_set_handler(evs_event_t *s_event, evs_event_handler handler)
{
	s_event->handler = handler;
}

void evs_event_send_empty_msg(evs_event_t *s_event, int what)
{
	evs_event_send_empty_msg_delay(s_event, what, 0);
}

void evs_event_send_empty_msg_delay(evs_event_t *s_event, int what, long delay)
{
	evs_event_message_t msg;
	memset(&msg, 0, sizeof(evs_event_message_t));
	msg.what = what;
	evs_event_send_msg_delay(s_event, &msg, delay);
}

void evs_event_send_msg(evs_event_t *s_event, evs_event_message_t *msg)
{
	evs_event_send_msg_delay(s_event, msg, 0);
}

void evs_event_send_msg_delay(evs_event_t *s_event, evs_event_message_t *msg, long delay)
{
	evs_event_message_wrap_t msg_wrap;
	memset(&msg_wrap, 0, sizeof(evs_event_message_wrap_t));
	evs_event_message_t *msg_copy =
			(evs_event_message_t *)evs_memp_malloc(sizeof(evs_event_message_t));
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "malloc failed!");
		return;
	}
	memcpy(msg_copy, msg, sizeof(evs_event_message_t));
	// LISA_LOGD(TAG, "evs_event_send_msg_delay user_data ptr: %p, copy user_data: %p",
	// msg->user_data, 		msg_copy->user_data);
	msg_wrap.msg = msg_copy;
	msg_wrap.delay = delay;
	// TODO
	msg_wrap.time = lisa_os_get_ticks() + delay;
	// msg_wrap.time = lisa_os_get_time() * OS_MSEC_PER_SEC + delay;
	// LISA_LOGD(TAG, "evs_event_send_msg_delay what: %d, delay: %ld, time: %d", 0, delay,
	// msg_wrap.time);
	// is_wait 添加volatile，同时去除锁，防止消息处理中，耽搁外部任务消息
	if (s_event->is_wait) {
		LISA_LOGI(TAG, "event is wait msg, wakeup");
		lisa_semaphore_give(s_event->semaphore);
	}

	if (lisa_queue_push(s_event->queue, &msg_wrap, sizeof(evs_event_message_wrap_t), 0) !=
			LISA_OK) {
		LISA_LOGE(TAG, "send empty msg error");
		evs_memp_free(msg_copy);
		return;
	}
	LISA_LOGE(TAG, "evs_event_send_msg_delay is ok");
}

void evs_event_post_runnable(evs_event_t *s_event, evs_event_runnable runnable, void *user_data)
{
	evs_event_post_runnable_delay(s_event, runnable, user_data, 0);
}

void evs_event_post_runnable_delay(
		evs_event_t *s_event, evs_event_runnable runnable, void *user_data, long delay)
{
	evs_event_message_t msg;
	memset(&msg, 0, sizeof(evs_event_message_t));
	msg.runnable = runnable;
	msg.user_data = user_data;
	// LISA_LOGD(TAG, "evs_event_post_runnable_delay user_data ptr: %p", user_data);
	evs_event_send_msg_delay(s_event, &msg, delay);
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
		evs_memp_free(msg_node->msg_wrap);
		evs_memp_free(msg_node);
	}
	evs_event_message_wrap_t msg_wrap;
	memset(&msg_wrap, 0, sizeof(evs_event_message_wrap_t));
	while (lisa_queue_pop(s_event->queue, &msg_wrap, sizeof(evs_event_message_wrap_t), 0) ==
			LISA_OK) {
		evs_memp_free(msg_wrap.msg);
	}
	lisa_queue_delete(s_event->queue);
	lisa_mutex_unlock(s_event->mutex);

	if (s_event->mutex != NULL) {
		lisa_mutex_delete(s_event->mutex);
	}
	if (s_event->semaphore != NULL) {
		lisa_semaphore_delete(s_event->semaphore);
	}
	evs_free(s_event);
}
