#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ic_message.h"
#include "ic_stream.h"
#include "ic_proxy.h"

#include "FreeRTOS.h"
#include "projdefs.h"
#include "task.h"

#define REC_CELL_SIZE    2
#define REC_CHANNELS     2
#define REC_FRM_SAMPS    256
#define REC_FRM_SIZE     (REC_FRM_SAMPS * REC_CHANNELS * REC_CELL_SIZE)

typedef enum
{
	ap2cp_play_stream_id = 0,
    ap2cp_record_stream_id,
} ic_stream_id_e;

typedef enum {
    MIC_CMD_START = 1,
    MIC_CMD_STOP,
    MIC_CMD_PAUSE,
    MIC_CMD_RESUME,
} mic_cmd_msg_e;

typedef struct __attribute__((packed)) {
    uint32_t func;
    uint32_t arg;
} mic_cmd_t;

static ICStream *s_record_stream;
static void *p_stream_frame_0;
static ICStream aadc_Stream_0;

int record_ctrl(mic_cmd_msg_e cmd)
{
    int ret = 0;
    mic_cmd_t data = {0};

    data.func = cmd;
    data.arg = 0;

    ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_MIC, IC_MESSAGE_MSG_TYPE_CMD, &data, sizeof(data));
    if (ret != 0) {
        printf("Error sending message: %d\n", ret);
    }

    return ret;
}

static void aadcstream_consumer_acquire_intercore_frame(void)
{
    int ret;

    // 获取并同步共享状态
    ICStream_Consumer_fetchRemote(s_record_stream);
    ICStream_Consumer_waitFrame(s_record_stream); // 统一等待数据帧
    // 失败则返回出错码, 无关远程
    ret = ICStream_Consumer_acquireFrame(s_record_stream, &p_stream_frame_0);
    if (ret != IC_OK) {
        printf("Error acquiring frame: %d\n", ret);
    }
}

static void aadcstream_consumer_release_intercore_frame(void)
{
    int ret;

    // 失败则返回出错码, 无关远程
    ret = ICStream_Consumer_releaseFrame(s_record_stream, p_stream_frame_0);
    if (ret != IC_OK) {
        printf("Error releasing frame: %d\n", ret);
    }

    // 发布最新状态至共享, 通信机制出错码
    ret = ICStream_Consumer_commitRemote(s_record_stream);
    if (ret != IC_OK) {
        printf("Error committing frame: %d\n", ret);
    }
}

void aadcservice_remote_sync(void)
{
    s_record_stream = IC_Proxy_getRemoteICStream(&aadc_Stream_0, ap2cp_record_stream_id);
    if (!s_record_stream) {
        printf("Error getting remote stream\n");
        return;
    }

    ICStream_Consumer_syncWithProducer(s_record_stream);
}

static void audio_recorder_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    aadcservice_remote_sync();

    record_ctrl(MIC_CMD_START);

    while (1) {
        aadcstream_consumer_acquire_intercore_frame();
        printf("Stream frame: %p\n", p_stream_frame_0);

        // Process the audio data here

        aadcstream_consumer_release_intercore_frame();
    }
}

int main(int argc, char **argv)
{
    printf("Hello, world! Audio ADC\n");

    ic_message_init();

    // 添加超时机制
    const int max_retries = 10;
    int retries = 0;
    do {
        int32_t data = 0;
        int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_MIC, IC_MESSAGE_MSG_TYPE_CMD, &data, sizeof(data));
        if (ret == 0) {
            break;
        } else {
            if (++retries >= max_retries) {
                printf("Error: Failed after %d attempts\\n", max_retries);
                return -1;
            }
            printf("[%d] check ap mic msg\n", retries);
            vTaskDelay(pdMS_TO_TICKS(100)); // 添加100ms延时
        }
    } while (1);

    audio_recorder_task(NULL);

    return 0;
}
