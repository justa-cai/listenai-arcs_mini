/**
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_SYS_EVENTS_H__
#define __LISAUI_SYS_EVENTS_H__
#include "ebus/ebus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_BUS_NAME "lisaui"
#define LISAUI_SYSTEM_NOTIFY_CHANNEL_NAME "system_notify"

#define LISAUI_SYS_EVENT_CHECKED_BUTTON_HOME (1 << 0)
#define LISAUI_SYS_EVENT_CHECKED_BUTTON_BACK (1 << 1)
#define LISAUI_SYS_EVENT_CHECKED_BUTTON_CLOSE (1 << 2)

typedef struct{
    uint32_t events;
    uint32_t payload_len;
    void *payload;
}lisaui_sys_event_message_t;

#define LISAUI_SYS_EVENT_MESSAGE_DEFINE_DEFAULT(message,event)\
lisaui_sys_event_message_t message = {\
    .events = event,\
    .payload_len = 0,\
    .payload = NULL\
};

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_SYS_EVENTS_H__ */
