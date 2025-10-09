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

#define TAG "audio_play"

static ICStream *s_play_stream;
static void *p_stream_frame_0;
static ICStream play_Stream_0;

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
    LISA_ASSERT(IC_OK == ret, "ic_stream acquire failed");
}

static void PlayStream_Producer_releaseInterCoreFrame(void)
{
    int ret;

    // 失败则返回出错码, 无关远程
    ret = ICStream_Producer_releaseFrame(s_play_stream, p_stream_frame_0);
    LISA_ASSERT(IC_OK == ret, "ic_stream_release failed, ret=%d", ret); 

    // 发布最新状态至共享, 通信机制出错码
    ret = ICStream_Producer_commitRemote(s_play_stream);
    LISA_ASSERT(IC_OK == ret, "ic stream producer commit remote failed");
}

void PlayService_remote_sync(void)
{
    // 挂接到已经创建好的核间数据流通道上

    // 此时server端的ICStream对象已就绪
    s_play_stream = IC_Proxy_getRemoteICStream(&play_Stream_0, ap2cp_play_stream_id);

    // 对象级的核间同步, 以确认对端已经就绪.
    ICStream_Producer_syncWithConsumer(s_play_stream);
}

void audio_play_service_start()
{
    PlayService_remote_sync();
}

void audio_play_send_pcm(char *data, int size)
{
    LISA_LOGV(TAG, "send pcm data %d", size);

    PlayStream_Producer_acquireInterCoreFrame();
    memcpy(p_stream_frame_0, data, size);
    PlayStream_Producer_releaseInterCoreFrame();
}