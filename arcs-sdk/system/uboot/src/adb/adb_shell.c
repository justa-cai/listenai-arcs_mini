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

#define CONFIG_BOOT_ADB_SHELL_BUFFER_SIZE    4096
#define CONFIG_BOOT_ADB_SHELL_LINE_BUFFER_SIZE 4096

#define SHELL_EVTS_IDLE BIT(0)
#define SHELL_EVTS_QUIT BIT(1)
#define SHELL_EVTS_XRDY BIT(2)

static struct adb_service *curr_service = NULL;

struct adb_shell_context {
    uint8_t buf[CONFIG_BOOT_ADB_SHELL_BUFFER_SIZE] __attribute__((aligned(8)));
    /* Keep one roomy line buffer for quoted shell commands and replies. */
    uint8_t line[CONFIG_BOOT_ADB_SHELL_LINE_BUFFER_SIZE];
    Shell sh;
    StreamBufferHandle_t rx_sbuf, tx_sbuf;
    TaskHandle_t task;
    EventGroupHandle_t evts;
    struct adb_service *service;
};

static signed short shell_write(char *data, unsigned short size)
{
    if (curr_service == NULL || data == NULL || size == 0)
        return 0;
    struct adb_shell_context *ctx = curr_service->data;
    return xStreamBufferSend(ctx->tx_sbuf, data, size, portMAX_DELAY);
}

void adb_shell_flush(void)
{
    if (curr_service == NULL) {
        return;
    }
    struct adb_shell_context *ctx = curr_service->data;

    size_t size = xStreamBufferReceive(ctx->tx_sbuf, ctx->line, sizeof(ctx->line), pdMS_TO_TICKS(0));

    if (size == 0) {
        return;
    }

    adb_service_write_remote(ctx->service, ctx->line, size);
}

static void shell_task(void *arg)
{
    struct adb_shell_context *ctx = arg;
    ADB_LOGI("adb shell task start, arg:%p", arg);

    ctx->rx_sbuf = xStreamBufferCreate(CONFIG_BOOT_ADB_SHELL_BUFFER_SIZE, 1);
    ADB_ASSERT(ctx->rx_sbuf);
    ctx->tx_sbuf = xStreamBufferCreate(CONFIG_BOOT_ADB_SHELL_BUFFER_SIZE, 1); 
    ADB_ASSERT(ctx->tx_sbuf);

    ctx->sh.read = NULL;
    ctx->sh.write = shell_write;
    shellInit(&ctx->sh, ctx->buf, CONFIG_BOOT_ADB_SHELL_BUFFER_SIZE);
    ADB_LOGD("shell task ready");

    xEventGroupSetBits(ctx->evts, SHELL_EVTS_XRDY);

    size_t size = 0;
    do {
        size = xStreamBufferReceive(ctx->rx_sbuf, ctx->line, sizeof(ctx->line), pdMS_TO_TICKS(50));
        for (int i = 0; i < size; i++)
            shellHandler(&ctx->sh, ctx->line[i]);
        size = xStreamBufferReceive(ctx->tx_sbuf, ctx->line, sizeof(ctx->line), pdMS_TO_TICKS(50));
        adb_service_write_remote(ctx->service, ctx->line, size);
    } while (!(xEventGroupGetBits(ctx->evts) & SHELL_EVTS_QUIT));
    while (true) {
        size = xStreamBufferReceive(ctx->tx_sbuf, ctx->line, sizeof(ctx->line), pdMS_TO_TICKS(50));
        if (size == 0) break;
        adb_service_write_remote(ctx->service, ctx->line, size);
    }

    // here, we just send CLSE-command to host, and do base feee(just free some stream objest), 
    // and host will replay CLSE-command to device later, then we can close the service totally.
    adb_close(ctx->service->local_id, ctx->service->remote_id);
    
    vStreamBufferDelete(ctx->rx_sbuf);
    vStreamBufferDelete(ctx->tx_sbuf);
    shellRemove(&ctx->sh);
    xEventGroupSetBits(ctx->evts, SHELL_EVTS_IDLE);
    ADB_LOGD("shell task quited!");
        
    vTaskDelete(NULL);
}

static int adb_shell_open(struct adb_service *service, const uint8_t *args)
{
    if (curr_service) {
        ADB_LOGE("adb shell already open");
        return -1;
    }

    ADB_LOGI("adb shell open, local_id:%d, remote_id:%d", service->local_id, service->remote_id);

    extern bool boot_handshake_is_need(void);
    extern bool boot_handshake_is_ok(void);
    extern void boot_handshake(bool ok);

    if (boot_handshake_is_need() && !boot_handshake_is_ok() && args) {
        extern const char *device_id_str_get(void);
        const char *code = device_id_str_get();
        if (strncmp(args, code, strlen(code)) == 0) {
            boot_handshake(true);
            args = "\r";
        } else {
            boot_handshake(false);
            return -1;
        }
    }

    if (args && strncmp(args, "1651470196", strlen("1651470196")) == 0) {
        /* 硬重启 */
        IP_AON_CTRL->REG_AON_SW_RESET.all = 0xCAFE000A;
        vTaskDelay(pdMS_TO_TICKS(100));
        __builtin_trap();
    }

    struct adb_shell_context *ctx = ADB_MALLOC(sizeof(struct adb_shell_context));
    ADB_ASSERT(ctx);
    memset(ctx, 0, sizeof(struct adb_shell_context));

    ctx->evts = xEventGroupCreate();
    ADB_ASSERT(ctx->evts);

    curr_service = service;
    ctx->service = service;
    service->data = ctx;

    BaseType_t xReturn = xTaskCreate(shell_task, "shell", OS_STACK_DEF, ctx, OS_PRIO_DEF, &ctx->task);
    ADB_ASSERT(xReturn == pdPASS);
    xEventGroupWaitBits(ctx->evts, SHELL_EVTS_XRDY, pdFALSE, pdFALSE, portMAX_DELAY);

    extern bool boot_handshake_is_need(void);

    if (args) {
        char *pargs = (char *)args;
        int len = strlen(pargs);
        ADB_LOGI("shell args: '%s', len: %d", pargs, len);
        if (len) {
            for (int j = 0; j < len; j++)
                if (pargs[j] == ';') pargs[j] = '\r';
            // we have pre-alloc some(4) extra bytes after the real data of packet
            pargs[len++] = '\r';
            // pargs[len++] = 0x03; // CTRL+C
            xStreamBufferSend(ctx->rx_sbuf, args, len, portMAX_DELAY);
            xEventGroupSetBits(ctx->evts, SHELL_EVTS_QUIT);
        }
    }

    return 0;
}

static int adb_shell_close(struct adb_service *service)
{
    ADB_LOGI("adb shell close, %p", service);
    if (service != NULL && service->data != NULL) {
        struct adb_shell_context *ctx = service->data;
        xEventGroupSync(ctx->evts, SHELL_EVTS_QUIT, SHELL_EVTS_IDLE, portMAX_DELAY);
        vEventGroupDelete(ctx->evts);
        ADB_FREE(ctx);
        service->data = NULL;
        curr_service = NULL;
        ADB_LOGI("shell exited!");
    }
    return 0;
}

static int adb_shell_write(struct adb_service *service, adb_packet_t *p)
{
    struct adb_shell_context *ctx = service->data;
    uint32_t len = p->datlen;
    if (len > 0) {
        uint8_t *data = p->data;
        if (len == 1 && data[0] == ETX) 
            xEventGroupSetBits(ctx->evts, SHELL_EVTS_QUIT);
        else
            xStreamBufferSend(ctx->rx_sbuf, data, len, portMAX_DELAY);
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
