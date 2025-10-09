#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ic_stream.h"
#include "ic_message.h"
#include "ic_stream.h"
#include "ic_proxy.h"
#include "rpc_client.h"
#include "audio_comm.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"

#define TAG "record"

static ICStream *s_record_stream;
static void *p_stream_frame_0;
static ICStream aadc_Stream_0;

#define UAS_REC_CELL_SIZE       2       // bytes, 16bit
#define UAS_REC_CHANNELS        5       // channels
#define UAS_REC_FRM_SAMPS       256     // samples/frame
#define UAS_REC_FRM_SIZE        (UAS_REC_FRM_SAMPS * UAS_REC_CHANNELS * UAS_REC_CELL_SIZE)

static void AadcStream_Consumer_acquireInterCoreFrame(void)
{
    int ret;

    // 获取最新的共享状态
    ICStream_Consumer_fetchRemote(s_record_stream);

    // 先检查是否有数据帧
    if (ICStream_Consumer_isEmpty(s_record_stream)) {
        // 等待数据帧出现, 如果已经有, 则立即返回, 否则阻塞等待让出调度
        ICStream_Consumer_waitFrame(s_record_stream);

        // 再获取最新的共享状态
        ICStream_Consumer_fetchRemote(s_record_stream);
    }
    else {
        // 就算内存先看见指针更新, 还是要等对端通知, 以保持流程同步
        ICStream_Consumer_waitFrame(s_record_stream);
    }
    // 失败则返回出错码, 无关远程
    ret = ICStream_Consumer_acquireFrame(s_record_stream, &p_stream_frame_0);
    LISA_ASSERT(IC_OK == ret, "");
}

static void AadcStream_Consumer_releaseInterCoreFrame(void)
{
    int ret;

    // 失败则返回出错码, 无关远程
    ret = ICStream_Consumer_releaseFrame(s_record_stream, p_stream_frame_0);
    LISA_ASSERT(IC_OK == ret, "");

    // 发布最新状态至共享, 通信机制出错码
    ret = ICStream_Consumer_commitRemote(s_record_stream);
    LISA_ASSERT(IC_OK == ret, "");
}

void AadcService_remote_sync(void)
{
    // 挂接到已经创建好的核间数据流通道上

    // 此时server端的ICStream对象已就绪
    s_record_stream = IC_Proxy_getRemoteICStream(&aadc_Stream_0, ap2cp_record_stream_id);

    // 对象级的核间同步, 以确认对端已经就绪.
    ICStream_Consumer_syncWithProducer(s_record_stream);
}

__attribute__((weak)) void handle_algo_record(const char *audio, int len)
{
    LISA_LOGV(TAG, "algo record: %d", len);
}

static void audio_recorder_thread(void *arg)
{
    while(1) {
		AadcStream_Consumer_acquireInterCoreFrame();
		// LISA_LOGD(TAG, "stream frame: %p", p_stream_frame_0);

        handle_algo_record(p_stream_frame_0, UAS_REC_FRM_SIZE);
        extern int app_usb_audio_write(void *data, uint32_t sample, uint32_t channel, uint8_t bit);
        app_usb_audio_write(p_stream_frame_0, UAS_REC_FRM_SAMPS, UAS_REC_CHANNELS, UAS_REC_CELL_SIZE * 8);
		AadcStream_Consumer_releaseInterCoreFrame();
	}
}

void audio_record_service_start()
{
    // Start Service
    AadcService_remote_sync();

    // Read Record with Thread
    lisa_thread_attr_t attr = {
        .stack_size = 8 * 1024,
        .priority = LISA_OS_PRIORITY_ABOVE_NORMAL,
        .name = (uint8_t *)"recorder",
    };

    lisa_thread_create(&attr, audio_recorder_thread, NULL);
}

