#include "xtts_service.h"
#include "xtts_stream.h"
#include "xtts_server.h"
#include "algo_lsf_client.h"

//------------------------------------------------
#include "ic_stream.h"
#include "ic_proxy.h"
#include "rpc_client.h"

//------------------------------------------------
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

//------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <string.h>

//------------------------------------------------
// #include "venus_ap.h"
// #include "cache.h"

//------------------------------------------------
// service与外部用户代码 连接

// XttsStream 数据流管道, open/close事件 通知对端, 数据流 一读一写
XttsStream _XttsService_xttsStream;

//------------------------------------------------
// 内部状态 - 配置
// observer
static XttsService_SignalEvent_t _XttsService_cb;
static uint32_t _XttsService_userData;

// 事件和状态迁移 ------------------------------------------------

enum XTTSSERVICE_MSG
{
    // 用户API事件
    XTTSSERVICE_MSG_START, // 开始运行服务
    XTTSSERVICE_MSG_STOP,  // 停止运行服务

    // 内部事件
};

// TODO: 需要同步cp侧扫描服务的实际状态么? 不用吧. 能获取结果就够.
enum XTTSSERVICE_STATE
{
    XTTSSERVICE_STATE_NO_INIT = 0, // 未初始化状态
    XTTSSERVICE_STATE_INITED,      // 已初始化

    XTTSSERVICE_STATE_RUNNING, // 运行中
};

// 单变量共享: 一写多读, api在用户task使用?
// TODO: 增加acquire/release barrier语义? COMPILER_BARRIER?
static volatile int _XttsService_state;

// cp端连接, 隐含成员 ------------------------------------------------

// ap <- cp, XttsStream中使用
ICStream *g_ic_stream_0;
static ICStream icStream_0;

// 私有成员 ------------------------------------------------

// 队列句柄 16 messages
static QueueHandle_t _XttsService_msgQ;

// 主task
static void _XttsService_task(void *arg);
// #define XTTSSERVICE_STACK_SIZE  (2048)

//------------------------------------------------

static void _XttsService_IC_connect(void);

//------------------------------------------------
// 接口

// resource acquisition is initialization
int32_t XttsService_initialize(void)
{
    // LOGD("[XttsService::api] XttsService_initialize\r\n");

    ASSERT(XTTSSERVICE_STATE_NO_INIT == _XttsService_state, "%s", __FUNCTION__);

    BaseType_t result;

    _XttsService_msgQ = xQueueCreate(16, sizeof(uint32_t));

    // TODO: task 栈 大小, 创建后台task
    result = xTaskCreate(_XttsService_task,   /* The function that implements the task. */
                         "_XttsService_task", /* Text name for the task. */
                         DEF_TASK_STACK,      /* Stack depth in words. */
                         NULL,                /* Task parameters. */
                         7,                  /* Priority and mode (user in this case). */
                         NULL                 /* Handle. */
    );
    ASSERT(pdPASS == result, "%s", __FUNCTION__);

    // 创建XttsStream管道, consumer端给外部, producer端给XttsServer
    XttsStream_ctor((void *)&_XttsService_xttsStream);

    // TODO 有了核间流, 不需要这个了吧, 远程调用还是要的?
    // 初始化cp服务
    XttsServer_initialize(&_XttsService_xttsStream);

    // 注册以监听: ocr识别通知
    XttsServer_register(NULL, 0);

    _XttsService_state = XTTSSERVICE_STATE_INITED;
    // LOGD("[XttsService::state] -> XTTSSERVICE_STATE_INITED\r\n");

    return XTTSSERVICE_OK;
}

// 用户task中使用, 异步开始, 不等待结果返回
// 在使能调度器之后, boot task中调用, 之前调用不行.
// service是os中的概念, 当然不能在os前存在.
int32_t XttsService_start(void)
{
    // LOGD("[XttsService::api] XttsService_start\r\n");

    ASSERT(XTTSSERVICE_STATE_INITED == _XttsService_state, "%s", __FUNCTION__);

    BaseType_t result;

    // 发送事件给service task: 开始运行服务
    uint32_t msg = XTTSSERVICE_MSG_START;
    result = xQueueSendToBack(_XttsService_msgQ, &msg, 0);
    ASSERT(pdPASS == result, "%s", __FUNCTION__);

    return XTTSSERVICE_OK;
}

// 请求发送至service的消息队列? 那么task未开始调度时, 无法使用了.
// 此处似乎有race condition? 但是start之后才会有callback, 所以没事吧.
// 或者, 用原子操作: 属性赋值, 或者队列入队
// 就算注册了, 真正的回调仍然要等到调度器运行起来后, 才能触发吧, 所以这里先记下.
// 不支持动态 register/unregister
int32_t XttsService_register(XttsService_SignalEvent_t cb_event, uint32_t user_data)
{
    LOGV("[XttsService::api] XttsService_register\r\n");

    _XttsService_cb = cb_event;
    _XttsService_userData = user_data;

    return XTTSSERVICE_OK;
}

// xtts task
static void _XttsService_task(void *arg)
{
    (void)arg;

    BaseType_t result;
    uint32_t msg;

    while (1)
    {
        result = xQueueReceive(_XttsService_msgQ, &msg, portMAX_DELAY);
        ASSERT(pdPASS == result, "%s", __FUNCTION__);

        switch (_XttsService_state)
        {
        // 初始态
        case XTTSSERVICE_STATE_INITED:
        {
            // 事件
            switch (msg)
            {
            // 开始运行服务
            case XTTSSERVICE_MSG_START:
            {
                // LOGD("[XttsService] XTTSSERVICE_MSG_START\r\n");

                // 连接cp服务, 配置而不启动 流
                // 之后可根据XttsStream的consumer端是否open, 来决定是否向其中生产 数据帧
                XttsServer_start();

                // XttsService运行中
                _XttsService_state = XTTSSERVICE_STATE_RUNNING;
                // LOGD("[XttsService::state] -> XTTSSERVICE_STATE_RUNNING\r\n");

                break;
            }

            default:
                LOGE("%s, %d", __FUNCTION__, __LINE__);
                break;
            }
            break;
        }

        // 运行中
        case XTTSSERVICE_STATE_RUNNING:
        {
            // 事件
            switch (msg)
            {
                // 目前没有事件需要处理
                // case XTTSSERVICE_MSG_something: {
                //     LOGD("[XttsService] XTTSSERVICE_MSG_something\r\n");

                //     break;
                // }

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

// 获取数据流管道引用, close 状态.
int32_t XttsService_getXttsStream(XttsStream **out_xttsStream)
{
    LOGD("[XttsService::api] XttsService_getXttsStream\r\n");

    // TODO: 根据XttsService状态, 共享单变量?
    ASSERT(XTTSSERVICE_STATE_INITED == _XttsService_state, "%s", __FUNCTION__);

    *out_xttsStream = &_XttsService_xttsStream;

    return 0;
}

void XttsService_remote_sync(void)
{
    // 挂接到已经创建好的核间数据流通道上

    // 此时server端的ICStream对象已就绪
    g_ic_stream_0 = IC_Proxy_getRemoteICStream(&icStream_0, cp2ap_xtts_stream_id);

    // 对象级的核间同步, 以确认对端已经就绪.
    ICStream_Consumer_syncWithProducer(g_ic_stream_0);

    return;
}
