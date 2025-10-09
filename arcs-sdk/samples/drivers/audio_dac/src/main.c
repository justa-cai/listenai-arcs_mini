#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "ic_message.h"
#include "ic_stream.h"
#include "ic_proxy.h"

#include "FreeRTOS.h"
#include "projdefs.h"
#include "task.h"

#define PLAYSTREAM_FRAME_SAMPLES    (768)

typedef enum
{
	ap2cp_play_stream_id = 0,
    ap2cp_record_stream_id,
} ic_stream_id_e;

static ICStream *s_play_stream;
static void *p_stream_frame_0;
static ICStream play_Stream_0;

static void generate_1khz_audio(int16_t *buffer, size_t length) {
    const double sample_rate = 16000.0;
    const double frequency = 1000.0;
    const double amplitude = 512.0; // 16位PCM的最大值

    for (size_t i = 0; i < length; i++) {
        double t = i / sample_rate;
        double value = amplitude * sin(2 * M_PI * frequency * t);
        buffer[i] = (int16_t)value;
    }
}



static void PlayStream_Producer_acquireInterCoreFrame(void)
{
    int ret;

    // 获取最新的共享状态
    ICStream_Producer_fetchRemote(s_play_stream);

    // 先检查是否有空帧
    if (ICStream_Producer_isFull(s_play_stream)) {
        // 等待空帧出现, 如果已经有, 则立即返回, 否则阻塞等待让出调度
        ICStream_Producer_waitFrame(s_play_stream);

        // 再获取最新的共享状态
        ICStream_Producer_fetchRemote(s_play_stream);
    }
    else {
        // 就算内存先看见指针更新, 还是要等对端通知, 以保持流程同步
        ICStream_Producer_waitFrame(s_play_stream);
    }
    // 失败则返回出错码, 无关远程
    ret = ICStream_Producer_acquireFrame(s_play_stream, &p_stream_frame_0);
    assert(IC_OK == ret);
}

static void PlayStream_Producer_releaseInterCoreFrame(void)
{
    int ret;

    // 失败则返回出错码, 无关远程
    ret = ICStream_Producer_releaseFrame(s_play_stream, p_stream_frame_0);
    assert(IC_OK == ret); 

    // 发布最新状态至共享, 通信机制出错码
    ret = ICStream_Producer_commitRemote(s_play_stream);
    assert(IC_OK == ret);
}

static void PlayService_remote_sync(void)
{
    // 挂接到已经创建好的核间数据流通道上

    // 此时server端的ICStream对象已就绪
    s_play_stream = IC_Proxy_getRemoteICStream(&play_Stream_0, ap2cp_play_stream_id);

    // 对象级的核间同步, 以确认对端已经就绪.
    ICStream_Producer_syncWithConsumer(s_play_stream);
}

static void audio_play_service_start()
{
    PlayService_remote_sync();
}

static void audio_play_send_pcm(char *data, int size)
{
    PlayStream_Producer_acquireInterCoreFrame();
    memcpy(p_stream_frame_0, data, size);
    PlayStream_Producer_releaseInterCoreFrame();
}

int main(int argc, char **argv)
{
    printf("Hello, world! Audio DAC\n\n");

    vTaskDelay(pdMS_TO_TICKS(50));
    // 初始化ic_message
    ic_message_init();

    vTaskDelay(pdMS_TO_TICKS(20));
    audio_play_service_start();

    short buffer[PLAYSTREAM_FRAME_SAMPLES*2];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        generate_1khz_audio(buffer, PLAYSTREAM_FRAME_SAMPLES);
        audio_play_send_pcm((char *)buffer, PLAYSTREAM_FRAME_SAMPLES);
    }

    return 0;
}
