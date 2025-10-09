#include "xtts_server.h"
#include "xtts_stream.h"
//------------------------------------------------
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

//------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 内外部通路------------------------------------------------

// 引用
static XttsStream *_XttsServer_xttsStream;

// 内部状态 - 配置 ------------------------------------------------
// observer
static XttsServer_SignalEvent_t _XttsServer_cb;
static uint32_t _XttsServer_userData;

static int _XttsServer_session;

// 事件和状态迁移 ------------------------------------------------

enum XTTSSERVER_MSG
{
    // 用户API事件
    XTTSSERVER_MSG_START, // 开始运行服务
    XTTSSERVER_MSG_STOP,  // 停止运行服务

    // 内部事件
    XTTSSERVER_MSG_STREAM_OPEN,  // XttsStream对端open
    XTTSSERVER_MSG_STREAM_CLOSE, // XttsStream对端close
};

// TODO: 需要同步cp侧扫描服务的实际状态么? 不用吧. 能获取结果就够.
enum XTTSSERVER_STATE
{
    XTTSSERVER_STATE_NO_INIT = 0, // 未初始化状态
    XTTSSERVER_STATE_INITED,      // 已初始化

    XTTSSERVER_STATE_RUNNING, // 运行中
};

// pending的事件, 运行时处理
static int _XttsServer_pending_open_stream;

// 单变量共享: 一写多读, api在用户task使用?
// TODO: 增加acquire/release barrier语义? COMPILER_BARRIER?
static volatile int _XttsServer_state;

// 附加状态
static int _XttsServer_state_stream_open;

// 私有成员 ------------------------------------------------

// 队列句柄 16 messages
static QueueHandle_t _XttsServer_msgQ;

// 主task
static void _XttsServer_task(void *arg);
// #define XTTSSERVER_STACK_SIZE  (2048)

// 实际图像才用? 或者只需要产生顺序增长的字节数据.
// static MEMSrc _XttsServer_memSrc;

// 接口 ------------------------------------------------

static uint32_t _XttsServer_xttsStreamProducerCB(uint32_t event, uint32_t param, uint32_t user_data)
{
    (void)param;
    (void)user_data;

    BaseType_t result;

    if (XTTSSTREAM_EVENT_CONSUMER_OPEN == event)
    {
        // 消息发给XttsServer的msgQ: consumer端已open
        // XttsServer根据inited/running: 做不同处理
        uint32_t msg = XTTSSERVER_MSG_STREAM_OPEN;
        result = xQueueSendToBack(_XttsServer_msgQ, &msg, 0);
        ASSERT(pdPASS == result, "%s", __FUNCTION__);
    }
    else if (XTTSSTREAM_EVENT_CONSUMER_CLOSE == event)
    {
        // 消息发给XttsServer的msgQ: consumer端已close
        // XttsServer根据inited/running: 做不同处理
        uint32_t msg = XTTSSERVER_MSG_STREAM_CLOSE;
        result = xQueueSendToBack(_XttsServer_msgQ, &msg, 0);
        ASSERT(pdPASS == result, "%s", __FUNCTION__);
    }

    return 0;
}

// server initialize. 给入XttsStream, 作为其producer
int32_t XttsServer_initialize(XttsStream *xttsStream)
{
    // LOGD("[XttsServer::api] XttsServer_initialize\r\n");

    ASSERT(XTTSSERVER_STATE_NO_INIT == _XttsServer_state, "%s", __FUNCTION__);

    BaseType_t result;

    _XttsServer_pending_open_stream = 0;
    _XttsServer_state_stream_open = 0;

    _XttsServer_xttsStream = xttsStream;

    // MEMSrc_ctor(& _XttsServer_memSrc);

    _XttsServer_msgQ = xQueueCreate(16, sizeof(uint32_t));

    // TODO: task 栈 大小, 创建后台task
    result = xTaskCreate(_XttsServer_task,   /* The function that implements the task. */
                         "_XttsServer_task", /* Text name for the task. */
                         DEF_TASK_STACK,     /* Stack depth in words. */
                         NULL,               /* Task parameters. */
                         7,                 /* Priority and mode (user in this case). */
                         NULL                /* Handle. */
    );
    ASSERT(pdPASS == result, "%s", __FUNCTION__);

    // 监听 XttsStream consumer端 的open/close事件,
    //     发回消息, 因为影响流程. 需要考虑, 消息的延后, 那么producer端可能填不进去.
    //          acquire时返回closed-error
    //     管道残留需要清理吧
    XttsStream_register(_XttsServer_xttsStreamProducerCB, 0);

    // TODO: 初始化cp服务? 但是XttsServer本身就代表了cp?

    _XttsServer_state = XTTSSERVER_STATE_INITED;
    // LOGD("[XttsServer::state] -> XTTSSERVER_STATE_INITED\r\n");

    return XTTSSERVER_OK;
}

int32_t XttsServer_register(XttsServer_SignalEvent_t cb_event, uint32_t user_data)
{
    // LOGD("[XttsServer::api] XttsServer_register\r\n");

    _XttsServer_cb = cb_event;
    _XttsServer_userData = user_data;

    return XTTSSERVER_OK;
}

// server start
int32_t XttsServer_start(void)
{
    // 开启后台task
    // LOGD("[XttsServer::api] XttsServer_start\r\n");

    ASSERT(XTTSSERVER_STATE_INITED == _XttsServer_state, "%s", __FUNCTION__);

    BaseType_t result;

    // 发事件给server task: 开始运行
    uint32_t msg = XTTSSERVER_MSG_START;
    result = xQueueSendToBack(_XttsServer_msgQ, &msg, 0);
    ASSERT(pdPASS == result, "%s", __FUNCTION__);

    return XTTSSERVER_OK;
}

static void _XttsServer_task(void *arg)
{
    (void)arg;

    BaseType_t result;
    uint32_t msg;

    while (1)
    {
        result = xQueueReceive(_XttsServer_msgQ, &msg, portMAX_DELAY);
        ASSERT(pdPASS == result, "%s", __FUNCTION__);

        switch (_XttsServer_state)
        {
        // 初始态
        case XTTSSERVER_STATE_INITED:
        {
            // 事件
            switch (msg)
            {
            // TODO: 新ocr结果. 可能为0长字符串. param 为字符串长度
            case XTTSSERVER_MSG_STREAM_OPEN:
            {
                // LOGD("[XttsServer] XTTSSERVER_MSG_STREAM_OPEN\r\n");

                // 未运行时的消息: 先记录, 待运行时处理
                _XttsServer_pending_open_stream = 1;

                break;
            }

            // 开始运行服务
            case XTTSSERVER_MSG_START:
            {
                // LOGD("[XttsServer] XTTSSERVER_MSG_START\r\n");

                // 连接cp扫描服务, 配置而不启动 raw图流
                // 之后可根据XttsStream的consumer端是否open, 来决定是否向其中生产图像帧

                if (_XttsServer_pending_open_stream)
                {
                    // 状态设置: 需要产生图流
                    _XttsServer_state_stream_open = 1;

                    // pending事件已处理, 复位
                    _XttsServer_pending_open_stream = 0;
                }

                // XttsServer运行中
                _XttsServer_state = XTTSSERVER_STATE_RUNNING;
                // LOGD("[XttsServer::state] -> XTTSSERVER_STATE_RUNNING\r\n");

                break;
            }

            default:
                LOGE("%s, %d", __FUNCTION__, __LINE__);
                break;
            }
            break;
        }
        // 运行中, 可能有stream open/close事件
        // TODO: mock版不需要处理
        case XTTSSERVER_STATE_RUNNING:
        {
            // 事件
            switch (msg)
            {
            case XTTSSERVER_MSG_STREAM_OPEN:
            {
                LOGD("[XttsServer] XTTSSERVER_MSG_STREAM_OPEN\r\n");

                break;
            }

            case XTTSSERVER_MSG_STREAM_CLOSE:
            {
                LOGD("[XttsServer] XTTSSERVER_MSG_STREAM_CLOSE\r\n");

                break;
            }

            default:
                LOGE("%s, %d", __FUNCTION__, __LINE__);
                break;
            }

            break;
        }

        default:
            LOGE("%s, %d", __FUNCTION__, __LINE__);
            break;
        }
    };

    return;
}
