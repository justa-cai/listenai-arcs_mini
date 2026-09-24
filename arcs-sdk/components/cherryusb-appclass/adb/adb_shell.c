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
#include <strings.h>

#define LOG_TAG "adb.sh"

#include "adb_utils.h"

#include "lisa_log.h"

#if CONFIG_ADB_SHELL_EARLY_LOG
#include "sysheap.h"
#include "sys_init.h"
#endif

#define ETX 0x03 /* ctrl+c */
#define ADB_SHELL_TX_ACK_TIMEOUT_MS 1000U

/* Recovery handshake hooks (boot only). App builds get the weak defaults
 * below, so the handshake gate in adb_shell_open() is a no-op outside
 * uboot recovery. boot_recovery_shell_cmds.c provides the strong symbols
 * that drive the actual handshake state machine. */
__attribute__((weak)) bool boot_handshake_is_need(void) { return false; }
__attribute__((weak)) bool boot_handshake_is_ok(void) { return false; }
__attribute__((weak)) void boot_handshake(bool ok) { (void)ok; }
__attribute__((weak)) const char *device_id_str_get(void) { return NULL; }

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
    SemaphoreHandle_t tx_ready_sem;
    volatile bool closing;
    bool exit_only;
    bool output_enabled;
    char *command;
};

static bool adb_shell_wait_tx_ready(struct adb_shell_context *ctx)
{
    if (xSemaphoreTake(ctx->tx_ready_sem,
                       pdMS_TO_TICKS(ADB_SHELL_TX_ACK_TIMEOUT_MS)) != pdTRUE) {
        ADB_LOGW("shell TX ACK timeout\n");
        return false;
    }

    return !ctx->closing;
}

static bool adb_shell_send_remote(struct adb_shell_context *ctx,
                                  uint8_t *data, size_t len)
{
    if (len == 0U || !adb_shell_wait_tx_ready(ctx)) {
        return false;
    }

    adb_service_write_remote(ctx->s, data, (int)len);
    return true;
}

static void adb_shell_log_output(const uint8_t *log, uint32_t len, void *data)
{
    if (curr_service == NULL || curr_service->data == NULL) {
        return;
    }

    struct adb_shell_context *ctx = curr_service->data;

    if (ctx->command != NULL) {
        /* Non-interactive sessions have no input line or prompt to redraw. */
        if (ctx->output_enabled) {
            xStreamBufferSend(ctx->tx_stream, log, len, 0);
        }
    } else {
        shellWriteEndLine(&ctx->sh, (char *)log, len);
    }
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
            adb_shell_send_remote(ctx, data, received);
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
    if (ctx->output_enabled) {
        xStreamBufferSend(ctx->tx_stream, data, size, 0);
    }

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
                adb_shell_send_remote(ctx, data, received);
            }
            ADB_FREE(data);
        }
    }
}

static bool shell_is_exit(const char *command)
{
    if (command == NULL) {
        return false;
    }
    command += strspn(command, " \t\r\n");
    if (strncmp(command, "exit", 4) != 0) {
        return false;
    }
    command += 4;
    return command[strspn(command, " \t\r\n")] == '\0';
}

static void shell_task(void *arg)
{
    struct adb_shell_context *ctx = arg;

    vTaskDelay(50 / portTICK_PERIOD_MS);
    /* Host completion scripts use `adb shell exit` as a liveness probe.
     * This is an empty session, not a command for the embedded shell. Do not
     * initialise its prompt or consume/forward the early boot log cache. */
    if (ctx->exit_only) {
        if (!ctx->closing) {
            adb_close(ctx->s->local_id, ctx->s->remote_id);
        }
        xSemaphoreGive(ctx->exit_sem);
        vTaskDelete(NULL);
        return;
    }
    if (ctx->command == NULL) {
        ADB_LOGI("adb shell task start, arg:%p\n", arg);
    }

    ctx->sh.read = NULL;
    ctx->sh.write = shell_write;

    /* shellInit also writes the greeting/prompt. Suppress just that output
     * for one-shot commands; direct execution has no keyboard echo. */
    ctx->output_enabled = ctx->command == NULL;
    shellInit(&ctx->sh, ctx->buf, CONFIG_ADB_SHELL_BUFFER_SIZE);
    ctx->output_enabled = true;

    lisa_log_backend_add("adb_shell", adb_shell_log_output, NULL);

#if CONFIG_ADB_SHELL_EARLY_LOG
    if (ctx->command == NULL) {
        early_log_flush();
    }
#endif

    if (ctx->command != NULL) {
        char *saveptr;
        char *token = strtok_r(ctx->command, ";", &saveptr);
        while (token != NULL && !ctx->closing) {
            if (shell_is_exit(token)) {
                break;
            }
            shellRunNonInteractive(&ctx->sh, token);
            shell_task_flush_tx(ctx);
            token = strtok_r(NULL, ";", &saveptr);
        }
        if (adb_shell_wait_tx_ready(ctx)) {
            adb_close(ctx->s->local_id, ctx->s->remote_id);
        }
        goto finished;
    }

    uint8_t ch;
    while (!ctx->closing) {
        if (xStreamBufferReceive(ctx->rx_stream, &ch, 1, 50) == 1) {
            if (ch == ETX) {
                /* flush remaining output before closing */
                shell_task_flush_tx(ctx);
                if (adb_shell_wait_tx_ready(ctx)) {
                    adb_close(ctx->s->local_id, ctx->s->remote_id);
                }
                break;
            } else {
                shellHandler(&ctx->sh, ch);
            }
        }

        shell_task_flush_tx(ctx);
    }

finished:
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
        xSemaphoreGive(ctx->tx_ready_sem);
        if (xSemaphoreTake(ctx->exit_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ADB_LOGE("shell task exit timeout, force delete\n");
            vTaskDelete(ctx->task);
            lisa_log_backend_remove("adb_shell");
            shellRemove(&ctx->sh);
        }

        vSemaphoreDelete(ctx->exit_sem);
        vSemaphoreDelete(ctx->tx_ready_sem);
        vStreamBufferDelete(ctx->rx_stream);
        vStreamBufferDelete(ctx->tx_stream);
        ADB_FREE(ctx->command);
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

    /* Recovery handshake gate. Active only when boot is in wdt-loop state
     * (boot_handshake_is_need() == true). The first shell open must carry
     * the device id (printed at boot as "serial num: ...") as args; on
     * match we mark handshake ok so conn_timer_callback skips the reboot
     * path. App builds use the weak default that returns false and bypass
     * this entirely.
     *
     * The adb_service_handle interface passes args as a bare pointer with
     * no length, but the gate only treats inputs as valid when args is
     * exactly code_len bytes followed by NUL. To stay safe even if a
     * malformed OPEN payload arrives without a terminator, we cap the
     * length probe at code_len + 1 via strnlen — anything longer than
     * the device id (or missing NUL within the window) trips the length
     * compare and is rejected before any further read.
     *
     * Comparison is case-insensitive: device_id_str_get() returns a
     * hex serial number and the human user typing it should not have
     * to mind the casing. */
    if (boot_handshake_is_need() && !boot_handshake_is_ok()) {
        const char *code = device_id_str_get();
        size_t code_len = (code != NULL) ? strlen(code) : 0;
        size_t args_len = (args != NULL) ? strnlen((const char *)args, code_len + 1) : 0;
        if (code_len == 0 ||
            args_len != code_len ||
            strncasecmp((const char *)args, code, code_len) != 0) {
            boot_handshake(false);
            ADB_LOGE("adb shell handshake rejected\n");
            return -1;
        }
        boot_handshake(true);
        args = (const uint8_t *)"";
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
    ctx->exit_only = shell_is_exit((const char *)args);


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

    ctx->tx_ready_sem = xSemaphoreCreateBinary();
    if (ctx->tx_ready_sem == NULL) {
        vSemaphoreDelete(ctx->exit_sem);
        ADB_FREE(ctx);
        ADB_LOGE("shell TX ready semaphore create failed\n");
        curr_service = NULL;
        s->data = NULL;
        return -1;
    }
    xSemaphoreGive(ctx->tx_ready_sem);

    ctx->rx_stream = xStreamBufferCreate(CONFIG_ADB_SHELL_BUFFER_SIZE, 1);
    if (ctx->rx_stream == NULL) {
        vSemaphoreDelete(ctx->tx_ready_sem);
        vSemaphoreDelete(ctx->exit_sem);
        ADB_FREE(ctx);
        ADB_LOGE("shell rx stream create failed\n");
        curr_service = NULL;
        return -1;
    }

    ctx->tx_stream = xStreamBufferCreate(CONFIG_ADB_SHELL_BUFFER_SIZE, 1);
    if (ctx->tx_stream == NULL) {
        vStreamBufferDelete(ctx->rx_stream);
        vSemaphoreDelete(ctx->tx_ready_sem);
        vSemaphoreDelete(ctx->exit_sem);
        ADB_FREE(ctx);
        ADB_LOGE("shell tx stream create failed\n");
        curr_service = NULL;
        return -1;
    }

    if (args != NULL && args[0] != '\0') {
        size_t len = strlen((const char *)args);
        ctx->command = ADB_MALLOC(len + 1);
        if (ctx->command == NULL) {
            vStreamBufferDelete(ctx->rx_stream);
            vStreamBufferDelete(ctx->tx_stream);
            vSemaphoreDelete(ctx->tx_ready_sem);
            vSemaphoreDelete(ctx->exit_sem);
            ADB_FREE(ctx);
            curr_service = NULL;
            s->data = NULL;
            return -1;
        }
        memcpy(ctx->command, args, len + 1);
    }

    /* 2048-word stack: `recovery exit` -> boot_control_store_set_recovery()
     * places a 4 KB sector buffer on the stack. */
    BaseType_t xReturn = xTaskCreate(shell_task, "shell_task", 1024 * 2, ctx, CONFIG_ADB_TASK_PRIORITY - 1, &ctx->task);
    if (xReturn != pdPASS) {
        vStreamBufferDelete(ctx->rx_stream);
        vStreamBufferDelete(ctx->tx_stream);
        vSemaphoreDelete(ctx->tx_ready_sem);
        vSemaphoreDelete(ctx->exit_sem);
        ADB_FREE(ctx->command);
        ADB_FREE(ctx);
        ADB_LOGE("shell task create failed\n");
        curr_service = NULL;
        return -1;
    }

    return 0;
}

static void adb_shell_ready(struct adb_service *s)
{
    if (s != NULL && s->data != NULL) {
        struct adb_shell_context *ctx = s->data;
        xSemaphoreGive(ctx->tx_ready_sem);
    }
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
        /* One-shot execution does not consume interactive keyboard input. */
        if (ctx->command == NULL) {
            xStreamBufferSend(ctx->rx_stream, &data[i], 1, portMAX_DELAY);
        }
    }

    adb_packet_free(p);

    return 0;
}

static const struct adb_service_handle adb_shell_handle = {
    .name = "shell",
    .open = adb_shell_open,
    .close = adb_shell_close,
    .write = adb_shell_write,
    .ready = adb_shell_ready,
};

void adb_shell_init(void)
{
    adb_service_hd_register(&adb_shell_handle);
}
