#ifndef LISA_EVS_EVENT_H
#define LISA_EVS_EVENT_H

typedef int (*evs_event_runnable)(void *user_data);

typedef struct evs_event_message_s {
	int what;
	int arg1;
	int arg2;
	evs_event_runnable runnable;
	void *user_data;
} evs_event_message_t;

typedef struct evs_event_s evs_event_t;

evs_event_t *evs_event_create();
int evs_event_post_runnable(evs_event_t *, evs_event_runnable runnable, void *user_data);
int evs_event_post_runnable_delay(evs_event_t *, evs_event_runnable runnable, void *user_data, long delay);
int evs_event_send_msg_delay(evs_event_t *, evs_event_message_t *msg, long delay);
void evs_event_destroy(evs_event_t *);

#endif
