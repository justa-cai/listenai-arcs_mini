#ifndef LISA_EVS_EVENT_H
#define LISA_EVS_EVENT_H

#define EVS_EVENT_NORMAL (0)
#define EVS_EVENT_NEST (1)  // runnable和handler中如果有嵌套发消息，返回此值

typedef int (*evs_event_runnable)(void *user_data);

typedef struct evs_event_message_s {
	int what;
	int arg1;
	int arg2;
	evs_event_runnable runnable;
	void *user_data;
} evs_event_message_t;

typedef struct evs_event_s evs_event_t;
typedef int (*evs_event_handler)(evs_event_message_t *);

evs_event_t *evs_event_create();
void evs_event_set_handler(evs_event_t *, evs_event_handler handler);

void evs_event_send_empty_msg(evs_event_t *, int what);
void evs_event_send_empty_msg_delay(evs_event_t *, int what, long delay);
void evs_event_send_msg(evs_event_t *, evs_event_message_t *msg);  // msg 和 user_data需要自己回收
void evs_event_send_msg_delay(evs_event_t *, evs_event_message_t *msg, long delay);

void evs_event_post_runnable(evs_event_t *, evs_event_runnable runnable, void *user_data);
void evs_event_post_runnable_delay(
		evs_event_t *, evs_event_runnable runnable, void *user_data, long delay);
void evs_event_destroy(evs_event_t *);

#endif
