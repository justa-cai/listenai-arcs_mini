#include "xutils.h"
#include "syslog.h"

#define TRACE_LINE_LIMIT 128
#define TRACE_BUFF_TOTAL 8192

static StreamBufferHandle_t sbuf = NULL;
static uint8_t line[TRACE_LINE_LIMIT + 1] = { 0 };

static void trace_task(void *args)
{
    // uint32_t last = xTaskGetTickCount();
    // int maxc = 0;
    while (true) {
        // maxc = __maxof(xStreamBufferBytesAvailable(sbuf), maxc);
        int size = xStreamBufferReceive(sbuf, line, TRACE_LINE_LIMIT, pdMS_TO_TICKS(100));
        if (size > 0)
            syslog_write(line, size);
        else {
            // uint32_t tick = xTaskGetTickCount();
            // if (tick - last >= 5000) {
            //     last = tick;
            //     tick = pdMS_TO_TICKS(tick) / 1000;
            //     TRACE("[%02d:%02d:%02d] max=%d", tick / 3600, (tick % 3600) / 60, tick % 60, maxc);
            // }
        }
    }
}

_ssize_t _write_r(struct _reent *ptr, int fd, void *buf, size_t cnt)
{
    size_t size = 0;
#if 0
    size = syslog_write(buf, cnt);
#else
    if (!sbuf)
        size = syslog_write(buf, cnt);
    else if (!xPortIsInsideInterrupt())
        size = xStreamBufferSend(sbuf, buf, cnt, pdMS_TO_TICKS(100));
    else {
        BaseType_t yield = pdFALSE;
        size = xStreamBufferSendFromISR(sbuf, buf, cnt, &yield);
        portYIELD_FROM_ISR(yield);
    }
#endif
    if (size != cnt) {
        const char *warn = COR_FG_RED "[TRUNC]" CRLF CORDEF;
        syslog_write(warn, strlen(warn));
    }
    return size;
}

void trace_flush(void)
{
    if (!xPortIsInsideInterrupt())
        vTaskDelay(pdMS_TO_TICKS(300)); // wait trace task to flush
    else {
        while (true) {
            size_t size = xStreamBufferReceiveFromISR(sbuf, line, sizeof(line), NULL);
            if (size == 0)
                break;
            syslog_write(line, size);
        }
        syslog_write(CRLF, strlen(CRLF));
    }
}

void trace_init(void)
{
    sbuf = xStreamBufferCreate(TRACE_BUFF_TOTAL, 1);
    ASSERT(sbuf, "sbuf fail");

    BaseType_t ret = xTaskCreate(trace_task, "trace", OS_STACK_DEF, NULL, OS_PRIO_DEF, NULL);
    ASSERT(ret == pdPASS, "trace fail");
}

