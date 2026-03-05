#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "lisa_log.h"
#include "syslog.h"
#include "sysheap.h"

#include "string.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

struct lisa_log_backend {
    struct lisa_log_backend *next;
    char *name;
    void (*output)(const uint8_t *log, uint32_t len, void *data);
    void (*output_panic)(const uint8_t *log, uint32_t len, void *data);
    void *data;
    bool enabled;
};

/* 只能有一个前端 */
static const struct lisa_log_frontend *lisa_log_frontend = NULL;

/* 可以有多个后端 */
static struct lisa_log_backend *lisa_log_backend_list = NULL;
static SemaphoreHandle_t lisa_log_backend_list_mutex = NULL;

static volatile bool g_panic_mode = false;

#define LISA_LOG_CAN_LOCK()   (!(xPortIsInsideInterrupt() || xPortIsInsideCritical()))
#define LISA_LOG_PANIC_MODE() (g_panic_mode)

static void lisa_log_output(const uint8_t *log, uint32_t len)
{
    static int32_t lock_count = 0;
    if (LISA_LOG_CAN_LOCK()) {
        lock_count++;
        if (lock_count == 1) {
            xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
        } else {
            /**
             * 出现这种情况, 说明日志输出过程中发生了assert,
             * 如在take mutex时发生assert, assert的输出又被重定向到日志系统中的情况
             */
        }
    }

    if (lisa_log_backend_list) {
        struct lisa_log_backend *backend = lisa_log_backend_list;
        while (backend) {
            if (backend->enabled) {
                if (LISA_LOG_PANIC_MODE() || lock_count > 1) {
                    if(backend->output_panic){
                        backend->output_panic(log, len, backend->data);
                    }
                } else {
                    if(backend->output){
                        backend->output(log, len, backend->data);
                    }
                    
                }
            }
            backend = backend->next;
        }
    }

    if (LISA_LOG_CAN_LOCK()) {
        lock_count--;
        if(lock_count == 0) {
            xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
        }
    }
}

int lisa_log_init(void)
{
    static uint8_t init_done = 0;
    if (init_done) {
        return 0;
    }
    init_done = 1;

    lisa_log_backend_list_mutex = xSemaphoreCreateRecursiveMutex();
    assert(lisa_log_backend_list_mutex != NULL);

    extern int lisa_log_backend_sys_init(void);
    lisa_log_backend_sys_init();

#if CONFIG_SDK_MODULE_EASYLOGGER
    extern const struct lisa_log_frontend lisa_log_frontend_easylog;
    lisa_log_frontend = &lisa_log_frontend_easylog;
#endif

    if (lisa_log_frontend) {
        if (lisa_log_frontend->init) {
            lisa_log_frontend->init(lisa_log_frontend);
        }
        assert(lisa_log_frontend->output_hook_set != NULL);
        lisa_log_frontend->output_hook_set(lisa_log_output);
    }

    return 0;
}

void log_flush(void)
{
    if (lisa_log_frontend && lisa_log_frontend->flush) {
        lisa_log_frontend->flush(lisa_log_frontend);
    }
}

void logDump(uint8_t *data, int len)
{
#if CONFIG_LOG_FRONTEND_EASYLOGGER
    elog_hexdump("logDump", 16, data, len);
#endif
}

void logHexDump(char *name, uint8_t width, uint8_t *data, int len)
{
#if CONFIG_LOG_FRONTEND_EASYLOGGER
    elog_hexdump(name, width, data, len);
#endif
}

void log_write(void *unused, char c)
{
}

void lisa_log_set_level(uint8_t lvl)
{
    if (lisa_log_frontend && lisa_log_frontend->level_set) {
        lisa_log_frontend->level_set(lisa_log_frontend, lvl);
    }
}

static struct lisa_log_backend *lisa_log_backend_create(const char *name, lisa_log_output_t output, lisa_log_output_t output_panic, void *data)
{
    if (!name || !output) {
        return NULL;
    }

    struct lisa_log_backend *backend = (struct lisa_log_backend *)exram_malloc(4, sizeof(struct lisa_log_backend));
    if (!backend) {
        return NULL;
    }

    backend->name = exram_malloc(4, strlen(name) + 1);
    if (!backend->name) {
        exram_free(backend);
        return NULL;
    }

    strcpy(backend->name, name);

    backend->output = output;
    backend->output_panic = output_panic;
    backend->data = data;
    backend->enabled = true;
    backend->next = NULL;

    return backend;
}

static int lisa_log_backend_destroy(struct lisa_log_backend *backend)
{
    if (!backend) {
        return -1;
    }

    exram_free(backend->name);
    exram_free(backend);

    return 0;
}

int lisa_log_backend_remove(const char *name)
{
    if (!name) {
        return -1;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    struct lisa_log_backend *prev = NULL;
    struct lisa_log_backend *curr = lisa_log_backend_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                lisa_log_backend_list = curr->next;
            }
            lisa_log_backend_destroy(curr);
            if (LISA_LOG_CAN_LOCK()) {
                xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
            }

            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return -1;
}

static struct lisa_log_backend *lisa_log_backend_find(const char *name)
{
    if (!name) {
        return NULL;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    struct lisa_log_backend *curr = lisa_log_backend_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (LISA_LOG_CAN_LOCK()) {
                xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
            }
            return curr;
        }
        curr = curr->next;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return NULL;
}

int lisa_log_backend_add(const char *name, lisa_log_output_t output, void *data)
{
    struct lisa_log_backend *backend = lisa_log_backend_find(name);
    if (backend) {
        return -1;
    }

    backend = lisa_log_backend_create(name, output, NULL, data);
    if (!backend) {
        return -1;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    backend->next = lisa_log_backend_list;
    lisa_log_backend_list = backend;

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return 0;
}

int lisa_log_backend_add_v2(const char *name, lisa_log_output_t output, lisa_log_output_t output_panic,
                            void *data)
{
    struct lisa_log_backend *backend = lisa_log_backend_find(name);
    if (backend) {
        return -1;
    }

    if (output == NULL && output_panic == NULL) {
        return -1;
    }

    backend = lisa_log_backend_create(name, output, output_panic, data);
    if (!backend) {
        return -1;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }

    backend->next = lisa_log_backend_list;
    lisa_log_backend_list = backend;

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return 0;
}

int lisa_log_backend_pause(const char *name)
{
    if (!name) {
        return -1;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    struct lisa_log_backend *curr = lisa_log_backend_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->enabled = false;
            if (LISA_LOG_CAN_LOCK()) {
                xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
            }
            return 0;
        }
        curr = curr->next;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return -1;
}

int lisa_log_backend_resume(const char *name)
{
    if (!name) {
        return -1;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    struct lisa_log_backend *curr = lisa_log_backend_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->enabled = true;
            if (LISA_LOG_CAN_LOCK()) {
                xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
            }
            return 0;
        }
        curr = curr->next;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return -1;
}

int lisa_log_backend_pause_all(void)
{
    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    struct lisa_log_backend *curr = lisa_log_backend_list;

    while (curr) {
        curr->enabled = false;
        curr = curr->next;
    }
    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return 0;
}

int lisa_log_backend_resume_all(void)
{
    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreTakeRecursive(lisa_log_backend_list_mutex, portMAX_DELAY);
    }
    struct lisa_log_backend *curr = lisa_log_backend_list;

    while (curr) {
        curr->enabled = true;
        curr = curr->next;
    }

    if (LISA_LOG_CAN_LOCK()) {
        xSemaphoreGiveRecursive(lisa_log_backend_list_mutex);
    }

    return 0;
}

void lisa_log_panic_mode_enable(void)
{
    g_panic_mode = true;
}

void lisa_log_panic_mode_disable(void)
{
    g_panic_mode = false;
}

bool lisa_log_is_panic_mode(void)
{
    return g_panic_mode;
}
