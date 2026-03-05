// Copyright 2024-2025 ListenAI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once


#include "log_print.h"
#include "ls_event.h"
#include "sys_arch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_TAG "EVENT "
#define EVENT_LOG(fmt, ...)      CLOG(EVENT_TAG fmt, ##__VA_ARGS__)
#define EVENT_LOGE(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_ERROR)  { CLOG(EVENT_TAG "ERR "fmt,##__VA_ARGS__);}} while(0)
#define EVENT_LOGW(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_WARN)   { CLOG(EVENT_TAG "WRN "fmt,##__VA_ARGS__);}} while(0)
#define EVENT_LOGI(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_INFO)   { CLOG(EVENT_TAG "INF "fmt,##__VA_ARGS__);}} while(0)
#define EVENT_LOGD(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_DEBUG)  { CLOG(EVENT_TAG "DBG "fmt,##__VA_ARGS__);}} while(0)


#define EVENT_QUEUE_SIZE      16
#ifndef CFG_EVENT_TASK_PRIORITY
#define EVENT_TASK_PRIORITY   9
#else
#define EVENT_TASK_PRIORITY   CFG_EVENT_TASK_PRIORITY
#endif

#ifndef CFG_EVENT_TASK_STACK_SIZE
#define EVENT_TASK_STACK_SIZE 512
#else
#define EVENT_TASK_STACK_SIZE CFG_EVENT_TASK_STACK_SIZE
#endif


typedef struct {
	event_cb_t event_cb;
	void *event_cb_arg;
} event_cb_node_t;


typedef struct {
	event_module_t event_module_id;
	event_cb_node_t cb_node;
	struct list_head next;
	struct list_head event_node_list;
	rtos_event pending;
} event_module_node_t;

typedef struct {
	int event_id;
	event_cb_node_t cb_node;
	struct list_head next;
} event_node_t;


typedef struct {
	event_module_t event_module_id;
	int event_id;
	void *event_data;
} event_info_t;

typedef struct {
	event_module_t event_module_id;
	int event_id;
	event_cb_t event_cb;
	void *event_cb_arg;
} event_register_info_t;

enum {
	EVENT_MSG_REGISTER = 0,
	EVENT_MSG_UNREGISTER,
	EVENT_MSG_DEINIT,
	EVENT_MSG_POST,
	EVENT_MSG_COUNT,
};

typedef struct {
	int msg_type;
	int  sync_msg_ret;
	rtos_semaphore sync_msg_sem;
	bool is_sync_msg;
	union {
		event_info_t event_info;
		event_register_info_t register_info;
	} msg;
} event_msg_t;

#ifdef __cplusplus
}
#endif
