#include "adb_services.h"
#include "adb.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "shell.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include <stdio.h>
#include <string.h>

#define LOG_TAG "adb.sh"

#include "adb_utils.h"

#define ETX 0x03 /* ctrl+c */

static struct adb_service *curr_service = NULL;

struct adb_shell_context {
    Shell sh;
    uint8_t buf[CONFIG_ADB_SHELL_BUFFER_SIZE] __attribute__((aligned(8)));
    QueueHandle_t rx_queue;
    QueueHandle_t tx_queue;
    struct adb_service *s;
    TaskHandle_t task;
};

static signed short shell_write(char *data, unsigned short size)
{
    if (curr_service == NULL || data == NULL || size == 0) {
        return 0;
    }
    struct adb_shell_context *ctx = curr_service->data;
    int len = size;
    while (len--) {
        xQueueSend(ctx->tx_queue, data++, 0);
    }

    return size;
}

static void shell_task(void *arg)
{
    struct adb_shell_context *ctx = arg;

    vTaskDelay(50 / portTICK_PERIOD_MS);
    ADB_LOGI("adb shell task start, arg:%p\n", arg);
    
    ctx->sh.read = NULL;
    ctx->sh.write = shell_write;
    
    shellInit(&ctx->sh, ctx->buf, CONFIG_ADB_SHELL_BUFFER_SIZE);
    ADB_LOGD("shell init done\n");

    uint8_t ch;
    while (1) {
        if (xQueueReceive(ctx->rx_queue, &ch, 50) == pdTRUE) {
            if (ch == ETX) {
                adb_close(ctx->s->local_id, ctx->s->remote_id);
            } else {
                shellHandler(&ctx->sh, ch);
            }
        }

        uint32_t len = uxQueueMessagesWaiting(ctx->tx_queue);
        if (len > 0) {
            uint8_t *data = ADB_MALLOC(len);
            if (data != NULL) {
                int i = len;
                uint8_t *p = data;
                while (i--) {
                    xQueueReceive(ctx->tx_queue, p, 0);
                    p++;
                }
                adb_service_write_remote(ctx->s, data, len);
                ADB_FREE(data);
            }
        }

    }
}

static int adb_shell_open(struct adb_service *s, const uint8_t *args)
{
    if (s == NULL) {
        return -1;
    }

    if (curr_service != NULL) {
        ADB_LOGE("adb shell already open\n");
        return -1;
    }

    struct adb_shell_context *ctx = ADB_MALLOC(sizeof(struct adb_shell_context));

    if (ctx == NULL) {
        return -1;
    }
    memset(ctx, 0, sizeof(struct adb_shell_context));


    ADB_LOGI("adb shell open, local_id:%d, remote_id:%d\n", s->local_id, s->remote_id);

    ctx->s = s;
    s->data = ctx;
    curr_service = s;

    ctx->rx_queue = xQueueCreate(CONFIG_ADB_SHELL_BUFFER_SIZE, sizeof(uint8_t));
    if (ctx->rx_queue == NULL) {
        ADB_FREE(ctx);
        ADB_LOGE("shell rx queue create failed\n");
        curr_service = NULL;
        return -1;
    }

    ctx->tx_queue = xQueueCreate(CONFIG_ADB_SHELL_BUFFER_SIZE, sizeof(uint8_t));
    if (ctx->tx_queue == NULL) {
        vQueueDelete(ctx->rx_queue);
        ADB_FREE(ctx);
        ADB_LOGE("shell tx queue create failed\n");
        curr_service = NULL;
        return -1;
    }

    BaseType_t xReturn = xTaskCreate(shell_task, "shell_task", 1024 * 1, ctx, CONFIG_ADB_TASK_PRIORITY - 1, &ctx->task);
    if (xReturn != pdPASS) {
        vQueueDelete(ctx->rx_queue);
        vQueueDelete(ctx->tx_queue);
        ADB_FREE(ctx);
        ADB_LOGE("shell task create failed\n");
        curr_service = NULL;
        return -1;
    }

    if (args) {
        int len = strlen((char *)args);
        ADB_LOGI("shell args: %s, len: %d\n", args, len);
        if (len) {
            char *token;
            char *saveptr;
            char *cmd = ADB_MALLOC(len + 1);
            if (cmd == NULL) {
                return -1;
            }
            memcpy(cmd, args, len);
            cmd[len] = '\0';
            token = strtok_r(cmd, ";", &saveptr);

            uint8_t c;
            while (token != NULL) {
                ADB_LOGI("shell cmd: %s\n", token);
                while (strlen(token)) {
                    xQueueSend(ctx->rx_queue, token++, portMAX_DELAY);
                }
                c = '\r';
                xQueueSend(ctx->rx_queue, &c, portMAX_DELAY);
                token = strtok_r(NULL, ";", &saveptr);
            }
            ADB_FREE(cmd);
            c = ETX;
            xQueueSend(ctx->rx_queue, &c, portMAX_DELAY);
        }
    }

    return 0;
}

static int adb_shell_close(struct adb_service *s)
{
    ADB_LOGI("adb shell close, %p\n", s);

    if (s != NULL && s->data != NULL) {
        struct adb_shell_context *ctx = s->data;
        vTaskSuspend(ctx->task);
        vTaskDelete(ctx->task);
        vQueueDelete(ctx->rx_queue);
        shellRemove(&ctx->sh);
        ADB_FREE(ctx);
        curr_service = NULL;
        s->data = NULL;
    }

    return 0;
}

static void adb_shell_write_remote(struct adb_service *s, uint8_t *data, int len)
{
    adb_service_write_remote(s, data, len);
}

int adb_shell_write_datas(const char *data, int size)
{
    struct adb_shell_context *ctx = curr_service->data;
    int len = size;

    if (curr_service == NULL || data == NULL || size == 0) {
        return 0;
    }

    while (len--) {
        if (*data == '\n') {
            uint8_t c = '\r';
            xQueueSend(ctx->tx_queue, &c, 0);
        }

        xQueueSend(ctx->tx_queue, data++, 0);
    }

    return size;
}

#include <stdarg.h>

int adb_printf(const char *format, ...)
{
    if (curr_service == NULL) {
        return 0;
    }
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len > 0) {
        return adb_shell_write_datas(buffer, len);
    }
    
    return 0;
}

static int adb_shell_write(struct adb_service *s, adb_packet_t *p)
{
    struct adb_shell_context *ctx = s->data;

    uint32_t len = p->msg.data_length;
    uint8_t *data = p->data;

    while (len--) {
        if (*data == ETX) {
            adb_service_close(s->local_id, s->remote_id);
            break;
        }

        xQueueSend(ctx->rx_queue, data, portMAX_DELAY);
        data++;
    }

    adb_packet_free(p);

    return 0;
}

static const struct adb_service_handle adb_shell_handle = {
    .name = "shell",
    .open = adb_shell_open,
    .close = adb_shell_close,
    .write = adb_shell_write,
};

void adb_shell_init(void)
{
    adb_service_hd_register(&adb_shell_handle);
}
