#ifndef ASYNC_TASK_H
#define ASYNC_TASK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct async_task async_task_t;
typedef void (*async_task_func_t)(void *user_data, bool *should_stop);
typedef void (*async_task_complete_callback_t)(void *user_data, bool completed, bool interrupted);

async_task_t *async_task_create(const char *name, uint32_t stack_size, uint32_t priority, async_task_func_t func,
                                async_task_complete_callback_t callback, void *user_data);

int async_task_start(async_task_t *task);
int async_task_stop(async_task_t *task);
bool async_task_is_running(async_task_t *task);
void async_task_destroy(async_task_t *task);

#ifdef __cplusplus
}
#endif

#endif
