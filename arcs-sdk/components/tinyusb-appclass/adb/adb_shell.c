#include "adb_services.h"
#include "adb.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "shell.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"

#include <stdio.h>
#include <string.h>

#define LOG_TAG "adb.sh"

#include "adb_utils.h"

#include "lisa_log.h"

#if CONFIG_ADB_SHELL_EARLY_LOG
#include "sysheap.h"
#include "sys_init.h"
#endif

#define ETX 0x03 /* ctrl+c */

static struct adb_service *curr_service = NULL;

/* ---- Early boot log caching ---- */
#if CONFIG_ADB_SHELL_EARLY_LOG

static struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t write_pos;
    uint32_t dropped;
    bool active;
} early_log;

static void early_log_output(const uint8_t *log, uint32_t len, void *data)
{
    if (!early_log.active || !early_log.buf) {
        return;
    }

    UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    uint32_t remaining = early_log.size - early_log.write_pos;
    if (len <= remaining) {
        memcpy(early_log.buf + early_log.write_pos, log, len);
        early_log.write_pos += len;
    } else {
        early_log.dropped++;
    }
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

static int early_log_backend_init(void)
{
    early_log.buf = psram_malloc(CONFIG_ADB_SHELL_EARLY_LOG_SIZE);
    if (!early_log.buf) {
        return -1;
    }
    early_log.size = CONFIG_ADB_SHELL_EARLY_LOG_SIZE;
    early_log.write_pos = 0;
    early_log.dropped = 0;
    early_log.active = true;
    lisa_log_backend_add("early_log", early_log_output, NULL);
    return 0;
}
SYS_INIT(early_log_backend_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);

#endif /* CONFIG_ADB_SHELL_EARLY_LOG */

struct adb_shell_context {
    Shell sh;
    uint8_t buf[CONFIG_ADB_SHELL_BUFFER_SIZE] __attribute__((aligned(8)));
    StreamBufferHandle_t rx_stream;
    StreamBufferHandle_t tx_stream;
    struct adb_service *s;
    TaskHandle_t task;
    SemaphoreHandle_t exit_sem;
    volatile bool closing;
};

static void adb_shell_log_output(const uint8_t *log, uint32_t len, void *data)
{
    if (curr_service == NULL || curr_service->data == NULL) {
        return;
    }

    struct adb_shell_context *ctx = curr_service->data;

    shellWriteEndLine(&ctx->sh, (char *)log, len);
}

/* ---- Early log flush (called after adb_shell backend is registered) ---- */
#if CONFIG_ADB_SHELL_EARLY_LOG

/*
 * Drain tx_stream manually since shell_task's main loop is not running
 * during flush. Without this, tx_stream (CONFIG_ADB_SHELL_BUFFER_SIZE)
 * fills up and xStreamBufferSend with timeout 0 silently drops data.
 */
static void early_log_drain_tx(struct adb_shell_context *ctx)
{
    size_t len = xStreamBufferBytesAvailable(ctx->tx_stream);
    while (len > 0) {
        uint8_t *data = ADB_MALLOC(len);
        if (data == NULL) {
            break;
        }
        size_t received = xStreamBufferReceive(ctx->tx_stream, data, len, 0);
        if (received > 0) {
            adb_service_write_remote(ctx->s, data, received);
        }
        ADB_FREE(data);
        len = xStreamBufferBytesAvailable(ctx->tx_stream);
    }
}

static void early_log_flush(void)
{
    if (!early_log.buf || curr_service == NULL || curr_service->data == NULL) {
        return;
    }

    struct adb_shell_context *ctx = curr_service->data;

    /* Remove early_log backend to stop caching */
    lisa_log_backend_remove("early_log");
    early_log.active = false;

    /* Pause adb_shell backend to prevent new logs interleaving with cached logs */
    lisa_log_backend_pause("adb_shell");

    /* Output header */
    char header[80];
    int hlen = snprintf(header, sizeof(header),
        "--- early boot log (%lu messages dropped) ---\r\n",
        (unsigned long)early_log.dropped);
    adb_shell_log_output((const uint8_t *)header, hlen, NULL);
    early_log_drain_tx(ctx);

    /* Output cached content in chunks, drain tx_stream between chunks */
    uint32_t offset = 0;
    while (offset < early_log.write_pos) {
        uint32_t chunk = early_log.write_pos - offset;
        if (chunk > 256) {
            chunk = 256;
        }
        adb_shell_log_output(early_log.buf + offset, chunk, NULL);
        offset += chunk;
        early_log_drain_tx(ctx);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Output footer */
    const char *footer = "--- end of early boot log ---\r\n";
    adb_shell_log_output((const uint8_t *)footer, strlen(footer), NULL);
    early_log_drain_tx(ctx);

    /* Resume adb_shell backend for normal real-time logging */
    lisa_log_backend_resume("adb_shell");

    /* Free buffer */
    psram_free(early_log.buf);
    early_log.buf = NULL;
}

#endif /* CONFIG_ADB_SHELL_EARLY_LOG */

static signed short shell_write(char *data, unsigned short size)
{
    if (curr_service == NULL || data == NULL || size == 0) {
        return 0;
    }
    struct adb_shell_context *ctx = curr_service->data;
    xStreamBufferSend(ctx->tx_stream, data, size, 0);

    return size;
}

static void shell_task_flush_tx(struct adb_shell_context *ctx)
{
    size_t len = xStreamBufferBytesAvailable(ctx->tx_stream);
    if (len > 0) {
        uint8_t *data = ADB_MALLOC(len);
        if (data != NULL) {
            size_t received = xStreamBufferReceive(ctx->tx_stream, data, len, 0);
            if (received > 0 && !ctx->closing) {
                adb_service_write_remote(ctx->s, data, received);
            }
            ADB_FREE(data);
        }
    }
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

    lisa_log_backend_add("adb_shell", adb_shell_log_output, NULL);

#if CONFIG_ADB_SHELL_EARLY_LOG
    early_log_flush();
#endif

    uint8_t ch;
    while (!ctx->closing) {
        if (xStreamBufferReceive(ctx->rx_stream, &ch, 1, 50) == 1) {
            if (ch == ETX) {
                /* flush remaining output before closing */
                shell_task_flush_tx(ctx);
                adb_close(ctx->s->local_id, ctx->s->remote_id);
                break;
            } else {
                shellHandler(&ctx->sh, ch);
            }
        }

        shell_task_flush_tx(ctx);
    }

    ADB_LOGI("adb shell task exit\n");
    lisa_log_backend_remove("adb_shell");
    shellRemove(&ctx->sh);
    xSemaphoreGive(ctx->exit_sem);
    vTaskDelete(NULL);
}

static int adb_shell_close(struct adb_service *s)
{
    ADB_LOGI("adb shell close, %p\n", s);

    if (s != NULL && s->data != NULL) {
        struct adb_shell_context *ctx = s->data;

        /* signal shell_task to exit and wait for it */
        ctx->closing = true;
        if (xSemaphoreTake(ctx->exit_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ADB_LOGE("shell task exit timeout, force delete\n");
            vTaskDelete(ctx->task);
            lisa_log_backend_remove("adb_shell");
            shellRemove(&ctx->sh);
        }

        vSemaphoreDelete(ctx->exit_sem);
        vStreamBufferDelete(ctx->rx_stream);
        vStreamBufferDelete(ctx->tx_stream);
        ADB_FREE(ctx);
        curr_service = NULL;
        s->data = NULL;
    }

    return 0;
}

static int adb_shell_open(struct adb_service *s, const uint8_t *args)
{
    if (s == NULL) {
        return -1;
    }

    if (curr_service != NULL) {
        ADB_LOGE("adb shell already open, close and reopen\n");
        adb_service_close(curr_service->local_id, curr_service->remote_id);
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

    ctx->exit_sem = xSemaphoreCreateBinary();
    if (ctx->exit_sem == NULL) {
        ADB_FREE(ctx);
        ADB_LOGE("shell exit sem create failed\n");
        curr_service = NULL;
        return -1;
    }

    ctx->rx_stream = xStreamBufferCreate(CONFIG_ADB_SHELL_BUFFER_SIZE, 1);
    if (ctx->rx_stream == NULL) {
        vSemaphoreDelete(ctx->exit_sem);
        ADB_FREE(ctx);
        ADB_LOGE("shell rx stream create failed\n");
        curr_service = NULL;
        return -1;
    }

    ctx->tx_stream = xStreamBufferCreate(CONFIG_ADB_SHELL_BUFFER_SIZE, 1);
    if (ctx->tx_stream == NULL) {
        vStreamBufferDelete(ctx->rx_stream);
        vSemaphoreDelete(ctx->exit_sem);
        ADB_FREE(ctx);
        ADB_LOGE("shell tx stream create failed\n");
        curr_service = NULL;
        return -1;
    }

    BaseType_t xReturn = xTaskCreate(shell_task, "shell_task", 1024 * 1, ctx, CONFIG_ADB_TASK_PRIORITY - 1, &ctx->task);
    if (xReturn != pdPASS) {
        vStreamBufferDelete(ctx->rx_stream);
        vStreamBufferDelete(ctx->tx_stream);
        vSemaphoreDelete(ctx->exit_sem);
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
                vTaskDelete(ctx->task);
                vStreamBufferDelete(ctx->rx_stream);
                vStreamBufferDelete(ctx->tx_stream);
                vSemaphoreDelete(ctx->exit_sem);
                ADB_FREE(ctx);
                curr_service = NULL;
                ADB_LOGE("shell cmd alloc failed\n");
                return -1;
            }
            memcpy(cmd, args, len);
            cmd[len] = '\0';
            token = strtok_r(cmd, ";", &saveptr);

            uint8_t c;
            while (token != NULL) {
                ADB_LOGI("shell cmd: %s\n", token);
                size_t token_len = strlen(token);
                xStreamBufferSend(ctx->rx_stream, token, token_len, portMAX_DELAY);
                c = '\r';
                xStreamBufferSend(ctx->rx_stream, &c, 1, portMAX_DELAY);
                token = strtok_r(NULL, ";", &saveptr);
            }
            ADB_FREE(cmd);
            c = ETX;
            xStreamBufferSend(ctx->rx_stream, &c, 1, portMAX_DELAY);
        }
    }

    return 0;
}

static void adb_shell_write_remote(struct adb_service *s, uint8_t *data, int len)
{
    adb_service_write_remote(s, data, len);
}

static int adb_shell_write_datas(const char *data, int size)
{
    if (curr_service == NULL || data == NULL || size == 0) {
        return 0;
    }

    struct adb_shell_context *ctx = curr_service->data;

    for (int i = 0; i < size; i++) {
        if (data[i] == '\n') {
            uint8_t c = '\r';
            xStreamBufferSend(ctx->tx_stream, &c, 1, 0);
        }
        xStreamBufferSend(ctx->tx_stream, &data[i], 1, 0);
    }

    return size;
}

void adb_shell_flush(void)
{
    if (curr_service == NULL || curr_service->data == NULL) {
        return;
    }

    shell_task_flush_tx(curr_service->data);
}

static int adb_shell_write(struct adb_service *s, adb_packet_t *p)
{
    struct adb_shell_context *ctx = s->data;

    uint32_t len = p->msg.data_length;
    uint8_t *data = p->data;

    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == ETX) {
            adb_packet_free(p);
            return -1;
        }
        xStreamBufferSend(ctx->rx_stream, &data[i], 1, portMAX_DELAY);
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
