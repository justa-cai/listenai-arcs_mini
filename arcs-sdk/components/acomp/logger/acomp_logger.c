#include <string.h>

#include "ipc/acomp_ipc.h"
#include "acomp_logger.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "acomp_stream_ipc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TAG "acomp_logger"
#include "lisa_log.h"

#define ACOMP_LOGGER_DEV_NAME "acomp.logger"
#define ACOMP_LOGGER_STREAM_CH_NAME "stream.logger"
#define ACOMP_LOGGER_STREAM_CHN (0)

/* 从 Kconfig 获取配置参数 */
#define ACOMP_LOGGER_RX_TASK_STACK_SIZE CONFIG_ACOMP_LOGGER_THREAD_STACK_SIZE
#define ACOMP_LOGGER_RX_TASK_PRIORITY CONFIG_ACOMP_LOGGER_THREAD_PRIORITY

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
    TaskHandle_t rx_task_handle;
    volatile bool rx_task_running;
    SemaphoreHandle_t rx_sem;  /* 接收信号量，用于自动触发模式 */
    acomp_logger_output_cb_t output_cb;  /* 自定义输出回调函数 */
} acomp_logger_handle_t;

static acomp_logger_handle_t *logger_handle = NULL;
static int _logger_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);
static int _logger_stream_ch_disable(int chn);
static void *_logger_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx);
static int _logger_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer);

/* 日志接收线程 */
static void logger_rx_task(void *param)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    int ret;
    acomp_logger_handle_t *handle = (acomp_logger_handle_t *)param;

#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
    LISA_LOGI(TAG, "Logger RX task started (auto kick mode, threshold=%d)",
              CONFIG_ACOMP_LOGGER_AUTO_KICK_BUFFER_THRESHOLD);
#else
    LISA_LOGI(TAG, "Logger RX task started (manual kick mode, poll interval=%dms)",
              CONFIG_ACOMP_LOGGER_MANUAL_POLL_INTERVAL_MS);
#endif

    while (handle->rx_task_running) {
#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
        /* 自动触发模式：等待来自回调的信号量 */
        xSemaphoreTake(handle->rx_sem, pdMS_TO_TICKS(100));
#else
        /* 手动触发模式：等待超时时间 */
        vTaskDelay(pdMS_TO_TICKS(CONFIG_ACOMP_LOGGER_MANUAL_POLL_INTERVAL_MS));
#endif

        /* 从 R2M 流中获取日志数据 */
        while (handle->rx_task_running) {
            buffer = _logger_stream_rx_buffer_get(ACOMP_LOGGER_STREAM_CHN, &len, &desc_idx);
            if (buffer != NULL && len > 0) {
                /* 使用自定义回调或默认方式输出日志 */
                if (handle->output_cb) {
                    /* 调用用户自定义的输出回调 */
                    handle->output_cb(buffer, len);
                }

                /* 释放缓冲区 */
                ret = _logger_stream_rx_buffer_release(ACOMP_LOGGER_STREAM_CHN, desc_idx, len, buffer);
                if (ret != ACOMP_ERR_OK) {
                    LISA_LOGW(TAG, "Failed to release buffer: %d", ret);
                }
            } else {
                /* 没有更多数据，跳出内层循环 */
                break;
            }
        }
    }

    LISA_LOGI(TAG, "Logger RX task exited");
    handle->rx_task_handle = NULL;
    vTaskDelete(NULL);
}

static void event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_logger_handle_t *handle = (acomp_logger_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {
        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE) {
            if (((void *)message->address != NULL) && (message->len > 0)) {
                acomp_ipc_stream_update_t *ipc_msg;
                uint32_t chn;
                ipc_msg = (acomp_ipc_stream_update_t *)message->address;
                chn = ipc_msg->index;
                if (chn < sizeof(handle->stream->ch) / sizeof(handle->stream->ch[0])) {
                    if(chn == ACOMP_LOGGER_STREAM_CHN) {
#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
                        /* 自动触发模式：发送信号量通知接收线程 */
                        if (handle->rx_sem != NULL) {
                            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                            xSemaphoreGiveFromISR(handle->rx_sem, &xHigherPriorityTaskWoken);
                            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                        }
#endif
                    }
                }
            }
        }
    }
}

/**
 * @brief 设置日志输出回调函数
 *
 * @param cb 输出回调函数，传入 NULL 则恢复默认输出方式
 * @return ACOMP_ERR_OK 成功
 * @return ACOMP_ERR_INVALID_STATE 组件未初始化
 */
int acomp_logger_set_output_callback(acomp_logger_output_cb_t cb)
{
    if (logger_handle == NULL) {
        LISA_LOGE(TAG, "Logger not initialized");
        return ACOMP_ERR_INVALID_STATE;
    }

    logger_handle->output_cb = cb;

    if (cb == NULL) {
        LISA_LOGI(TAG, "Output callback cleared, using default output");
    } else {
        LISA_LOGI(TAG, "Output callback set to %p", cb);
    }

    return ACOMP_ERR_OK;
}

int acomp_logger_init(void)
{
    int ret = 0;

    if (logger_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    logger_handle = (acomp_logger_handle_t *)psram_malloc(sizeof(acomp_logger_handle_t));
    if (logger_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(logger_handle, 0, sizeof(acomp_logger_handle_t));
    logger_handle->event_callbacks = gcl_cb_list_create();
    logger_handle->dev_index = acomp_ipc_get_dev_index(ACOMP_LOGGER_DEV_NAME);

    if (logger_handle->dev_index < 0) {
        psram_free(logger_handle);
        logger_handle = NULL;
        LISA_LOGE(TAG, "acomp logger dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    LISA_LOGI(TAG, "acomp logger dev index %d, name:%s", logger_handle->dev_index, ACOMP_LOGGER_DEV_NAME);

#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
    /* 自动触发模式：创建二值信号量 */
    logger_handle->rx_sem = xSemaphoreCreateBinary();
    if (logger_handle->rx_sem == NULL) {
        LISA_LOGE(TAG, "Failed to create rx semaphore");
        psram_free(logger_handle);
        logger_handle = NULL;
        return ACOMP_ERR_NO_MEM;
    }
#endif

    ret = acomp_ipc_add_callback(logger_handle->dev_index, (ipc_event_cb_t)event_callback, logger_handle);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(logger_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    logger_handle->stream = acomp_stream_create(logger_handle->dev_index);
    if (logger_handle->stream == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }

    return 0;
}


static int _logger_prepare(void)
{
    int ret;

    ret = acomp_ipc_build_frame_send_sync(logger_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, NULL, 0);
    return ret;
}



int acomp_logger_start(void)
{
    int ret;
    acomp_stream_chn_create_desc_t stream_desc = {
        .cname = "logger.stream.0",
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = 0,
        .buffer_size = CONFIG_ACOMP_LOGGER_STREAM_BUFFER_SIZE,
        .num_descs = CONFIG_ACOMP_LOGGER_STREAM_BUFFER_COUNT,
#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
        .kick_policy = CONFIG_ACOMP_LOGGER_AUTO_KICK_BUFFER_THRESHOLD,  /* 自动触发 */
#else
        .kick_policy = 0,  /* 手动触发 */
#endif
    };
    BaseType_t task_ret;

    if (logger_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    /* 准备资源 */
    _logger_prepare();


    /* 启用流通道 */
    ret = _logger_stream_ch_enable(ACOMP_LOGGER_STREAM_CHN, &stream_desc);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Failed to enable stream channel: %d", ret);
        return ret;
    }

    /* 创建日志接收线程 */
    logger_handle->rx_task_running = true;
    task_ret = xTaskCreate(logger_rx_task, "logger_rx", ACOMP_LOGGER_RX_TASK_STACK_SIZE,
                           logger_handle, ACOMP_LOGGER_RX_TASK_PRIORITY, &logger_handle->rx_task_handle);
    if (task_ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create logger RX task");
        logger_handle->rx_task_running = false;
        _logger_stream_ch_disable(ACOMP_LOGGER_STREAM_CHN);
        return ACOMP_ERR_NO_MEM;
    }
    /* 启动 remote 端的 logger */
    ret = acomp_ipc_build_frame_send_sync(logger_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
        ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Failed to start remote logger: %d", ret);
        _logger_stream_ch_disable(ACOMP_LOGGER_STREAM_CHN);
        return ret;
    }

    LISA_LOGI(TAG, "Logger started successfully (buffer_size=%d, buffer_count=%d)",
              CONFIG_ACOMP_LOGGER_STREAM_BUFFER_SIZE, CONFIG_ACOMP_LOGGER_STREAM_BUFFER_COUNT);
    return ACOMP_ERR_OK;
}

int acomp_logger_stop(void)
{
    int ret;

    if (logger_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    /* 停止接收线程 */
    if (logger_handle->rx_task_handle != NULL) {
        logger_handle->rx_task_running = false;

#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
        /* 自动触发模式：发送信号量以唤醒阻塞的线程，使其能够退出 */
        if (logger_handle->rx_sem != NULL) {
            xSemaphoreGive(logger_handle->rx_sem);
        }
#endif

        /* 等待任务退出，最多等待 1 秒 */
        int wait_count = 100;
        while (logger_handle->rx_task_handle != NULL && wait_count-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (logger_handle->rx_task_handle != NULL) {
            LISA_LOGW(TAG, "Logger RX task did not exit gracefully, force delete");
            vTaskDelete(logger_handle->rx_task_handle);
            logger_handle->rx_task_handle = NULL;
        }
    }

#ifdef CONFIG_ACOMP_LOGGER_KICK_AUTO
    /* 自动触发模式：删除信号量 */
    if (logger_handle->rx_sem != NULL) {
        vSemaphoreDelete(logger_handle->rx_sem);
        logger_handle->rx_sem = NULL;
    }
#endif

    /* 停止 remote 端的 logger */
    ret = acomp_ipc_build_frame_send_sync(logger_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGW(TAG, "Failed to stop remote logger: %d", ret);
    }

    /* 禁用流通道 */
    _logger_stream_ch_disable(ACOMP_LOGGER_STREAM_CHN);

    LISA_LOGI(TAG, "Logger stopped");
    return ACOMP_ERR_OK;
}

static int _logger_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc)
{
    int ret = 0;

    if ((logger_handle == NULL) || (logger_handle->stream == NULL)) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    logger_handle->stream->ch[chn] =
        acomp_stream_ipc_channel_create(logger_handle->stream, chn, logger_handle->dev_index, desc);
    if (logger_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG, "acomp_logger_stream_ch_enable chn(%s) index(%d), desc(%p)", desc->cname, chn, desc);
    return ret;
}

int _logger_stream_ch_disable(int chn)
{
    int ret;
    if (logger_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(chn);
    LISA_LOGI(TAG, "acomp_logger_stream_ch_disable chn index(%d), ret(%d)", chn, ret);
    logger_handle->stream->ch[chn] = NULL;
    return ret;
}

static void *_logger_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx)
{
    uint8_t *ptr;

    if (logger_handle == NULL) {
        return NULL;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return NULL;
    }

    if (logger_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    ptr = logger_handle->stream->ops.rx_buffer_get(logger_handle->stream->ch[chn], len, desc_idx);
    return ptr;
}

static int _logger_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer)
{
    int ret;

    if (logger_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = logger_handle->stream->ops.rx_buffer_release(logger_handle->stream->ch[chn], buffer, len, desc_idx);

    return ret;
}
