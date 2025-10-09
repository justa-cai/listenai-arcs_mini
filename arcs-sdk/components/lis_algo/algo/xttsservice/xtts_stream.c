#include "xtts_stream.h"

//------------------------------------------------
#include "ic_stream.h"

//------------------------------------------------
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//------------------------------------------------
#include "FreeRTOS.h"
#include "semphr.h"

//------------------------------------------------
#undef LOGD
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)

//------------------------------------------------
// xtts pcm数据帧
// 空间大小4帧, 实际可用为(4-1=3)帧, 信号量按3帧
#define XTTSSTREAM_FRAME_COUNT      (3)
// 160*2 bytes/frame
#define XTTSSTREAM_BUF_BYTES        (160 * XTTSSTREAM_FRAME_HEIGHT * XTTSSTREAM_FRAME_COUNT)

extern ICStream *g_ic_stream_0;
static void *p_stream_frame_0;

// observer ------------------------------------------------
// 只需要通知producer端: 接收consumer端的open/close事件

static XttsStream_SignalEvent_t _XttsStream_cb;
static uint32_t _XttsStream_userData;

 // TODO: 增加acquire/release barrier语义? COMPILER_BARRIER?
static volatile int _XttsStream_isConsumerOpen; // 1: open; 0: close;

// 状态 ------------------------------------------------

enum XTTSSTREAM_STATE {
    XTTSSTREAM_STATE_NO_INIT = 0,   // 未初始化状态
    XTTSSTREAM_STATE_IDLE,          // 会话空闲
    XTTSSTREAM_STATE_PREPARE,       // 会话起头
    XTTSSTREAM_STATE_NORMAL,        // 正常数据中
};

int _XttsStream_producerState;
int _XttsStream_consumerState;

// 帧类型 ------------------------------------------------

#define XTTSSTREAM_PATTERN_BEGIN_OF_STREAM  (0x7b)
#define XTTSSTREAM_PATTERN_END_OF_STREAM    (0x5a)

// utils ------------------------------------------------

static int XttsStream_setFrameType(void *pFrame, int frameType);
static int XttsStream_checkFrameType(void *pFrame, int *out_frameType);

// API ------------------------------------------------

XttsStream * XttsStream_ctor(void *self_mem)
{
    LOGD("[XttsStream::api] XttsStream_ctor\r\n");

    _XttsStream_isConsumerOpen = 0;

    _XttsStream_consumerState = XTTSSTREAM_STATE_IDLE;

    return (XttsStream *) self_mem;
}

int32_t XttsStream_register(XttsStream_SignalEvent_t cb_event, uint32_t user_data)
{
    // 记录 监听回调, 以便对端变化及时获知
    LOGD("[XttsStream::api] XttsStream_register\r\n");

    _XttsStream_cb = cb_event;
    _XttsStream_userData = user_data;

    return XTTSSTREAM_OK;
}

int32_t XttsStream_open(XttsStream *self)
{
    (void) self;

    LOGD("[XttsStream::api] XttsStream_open\r\n");

    // consumer端: 标记设置 open
    _XttsStream_isConsumerOpen = 1;

    // open事件, 通知给producer端cb
    if (NULL != _XttsStream_cb) {
        _XttsStream_cb(XTTSSTREAM_EVENT_CONSUMER_OPEN, 0, _XttsStream_userData);
    }

    return 0;
}

int32_t XttsStream_close(XttsStream *self)
{
    (void) self;

    LOGD("[XttsStream::api] XttsStream_close\r\n");

    // consumer端: 标记设置 close
    _XttsStream_isConsumerOpen = 0;

    // close事件, 通知给producer端cb
    if (NULL != _XttsStream_cb) {
        _XttsStream_cb(XTTSSTREAM_EVENT_CONSUMER_CLOSE, 0, _XttsStream_userData);
    }

    return 0;
}

// TODO 传出参数 void **out_frame
static void XttsStream_Consumer_acquireInterCoreFrame(void)
{
    int ret;

    // 获取最新的共享状态
    ICStream_Consumer_fetchRemote(g_ic_stream_0);

    // 先检查是否有数据帧
    if (ICStream_Consumer_isEmpty(g_ic_stream_0)) {
        // 等待数据帧出现, 如果已经有, 则立即返回, 否则阻塞等待让出调度
        // TODO: 通信机制出错码
        ICStream_Consumer_waitFrame(g_ic_stream_0);

        // 再获取最新的共享状态
        ICStream_Consumer_fetchRemote(g_ic_stream_0);
    }
    else {
        // 就算内存先看见指针更新, 还是要等对端通知, 以保持流程同步
        ICStream_Consumer_waitFrame(g_ic_stream_0);
    }
    // 失败则返回出错码, 无关远程
    ret = ICStream_Consumer_acquireFrame(g_ic_stream_0, & p_stream_frame_0);
    ASSERT(IC_OK == ret, "");

    return;
}

// TODO 传出参数 void **out_frame
static void XttsStream_Consumer_releaseInterCoreFrame(void)
{
    int ret;

    // 失败则返回出错码, 无关远程
    ret = ICStream_Consumer_releaseFrame(g_ic_stream_0, p_stream_frame_0);
    ASSERT(IC_OK == ret, "");

    // 发布最新状态至共享, 通信机制出错码
    ret = ICStream_Consumer_commitRemote(g_ic_stream_0);
    ASSERT(IC_OK == ret, "");

    return;
}


// TODO: return: 已关闭; ok
int32_t XttsStream_Consumer_acquireFrame(XttsStream *self, void **out_frame, int *out_frameType)
{
    // TODO: 非mock直接接到核间流的信号量

    BaseType_t retOS;
    int ret;
    void *frame; // TODO 多余
    int frameType;

    // LOGV("[XttsStream::api] XttsStream_Consumer_acquireFrame\r\n");

    if (XTTSSTREAM_STATE_IDLE == _XttsStream_consumerState) {

        XttsStream_Consumer_acquireInterCoreFrame();
        frame = p_stream_frame_0;

        // 必须是BOS, 略过这帧, 再拿1帧
        XttsStream_checkFrameType(frame, & frameType);
        // TODO: 内部使用的frame类型 最好 与给外部的frame类型区分开, 目前是混用的
        ASSERT(XTTSSTREAM_FRAMETYPE_BEGIN_OF_STREAM == frameType, "");

        XttsStream_Consumer_releaseInterCoreFrame();

        _XttsStream_consumerState = XTTSSTREAM_STATE_PREPARE;

        XttsStream_Consumer_acquireInterCoreFrame();
        frame = p_stream_frame_0;

        // 必须是数据帧
        XttsStream_checkFrameType(frame, & frameType);
        ASSERT(XTTSSTREAM_FRAMETYPE_NORMAL == frameType, "");

        *out_frameType = XTTSSTREAM_FRAMETYPE_BEGIN_OF_STREAM;
        *out_frame = frame;

        // TODO 状态更新的位置有点怪, 似乎应该放在1帧已release后
        _XttsStream_consumerState = XTTSSTREAM_STATE_NORMAL;

        return 0;
    }

    ASSERT(XTTSSTREAM_STATE_NORMAL == _XttsStream_consumerState, "");
    // case 状态: XTTSSTREAM_STATE_NORMAL
    //     wait: 信号量: 数据frame
    //     audioFrame_acquire_frame
    //     如果数据帧
    //         frameType = XTTSSTREAM_FRAMETYPE_NORMAL;
    //     如果这是EOS
    //         nothing
    //         frameType = XTTSSTREAM_FRAMETYPE_END_OF_STREAM;
    //         状态 = XTTSSTREAM_STATE_IDLE

    XttsStream_Consumer_acquireInterCoreFrame();
    frame = p_stream_frame_0;

    XttsStream_checkFrameType(frame, & frameType);
    // TODO: 内部使用的frame类型 最好 与给外部的frame类型区分开, 目前是混用的

    if (XTTSSTREAM_FRAMETYPE_NORMAL == frameType) {
        *out_frameType = XTTSSTREAM_FRAMETYPE_NORMAL;
        *out_frame = frame;

        // 状态不变
    }
    else if (XTTSSTREAM_FRAMETYPE_END_OF_STREAM == frameType) {
        *out_frameType = XTTSSTREAM_FRAMETYPE_END_OF_STREAM;
        *out_frame = frame;

        // 状态变idle
        _XttsStream_consumerState = XTTSSTREAM_STATE_IDLE;
    }

    return 0;
}

int32_t XttsStream_Consumer_releaseFrame(XttsStream *self, void *frame)
{
    // TODO: 非mock直接接到核间流的信号量

    BaseType_t retOS;
    int ret;

    // LOGV("[XttsStream::api] XttsStream_Consumer_releaseFrame\r\n");

    XttsStream_Consumer_releaseInterCoreFrame();

    return 0;
}

// BOS/DATA/EOS
static int XttsStream_checkFrameType(void *pFrame, int *out_frameType)
{
    bool bos_found = true;
    bool eos_found = true;

    // LOGV("[XttsStream] XttsStream_checkFrameType\r\n");

    // 尝试查找BOS标记
    uint8_t *pUint8 = (uint8_t *) pFrame;
    for (int i = 0; i < 8; ++i) {
        if (pUint8[i] != XTTSSTREAM_PATTERN_BEGIN_OF_STREAM) {
            bos_found = false;
            break;
        }
    }
    if (bos_found) {
        *out_frameType = XTTSSTREAM_FRAMETYPE_BEGIN_OF_STREAM;
        LOGD("[XttsStream] type: BOS\r\n");
        return 0;
    }

    // 尝试查找EOS标记
    pUint8 = (uint8_t *) pFrame;
    for (int i = 0; i < 8; ++i) {
        if (pUint8[i] != XTTSSTREAM_PATTERN_END_OF_STREAM) {
            eos_found = false;
            break;
        }
    }
    if (eos_found) {
        *out_frameType = XTTSSTREAM_FRAMETYPE_END_OF_STREAM;
        LOGD("[XttsStream] type: EOS\r\n");
        return 0;
    }

    // 只能是DATA帧
    *out_frameType = XTTSSTREAM_FRAMETYPE_NORMAL;
    // LOGD("[XttsStream] type: NORMAL\r\n");

    return 0;
}
