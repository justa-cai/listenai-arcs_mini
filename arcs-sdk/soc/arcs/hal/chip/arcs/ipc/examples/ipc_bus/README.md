# IPC Bus 使用指南

IPC Bus 是构建在 MRPC 之上的上层跨核消息总线，让一个核上的代码用「服务 + 消息」的方式调用另一个核，并屏蔽底层通道、分包、收发细节。

本文档配合本目录下的 [`ipc_bus_demo.c`](ipc_bus_demo.c) 阅读，看完即可上手。

---

## 1. 核心概念

| 概念 | 说明 |
|------|------|
| **service_id** | 服务标识（非 0）。一个服务下挂多个消息处理函数。 |
| **msg_id** | 服务内的消息标识，区分同一服务的不同操作。 |
| **version** | 服务版本，用于收发两端协商；不关心版本时填 `IPC_BUS_SERVICE_VERSION_ANY`。 |
| **handler** | 处理函数 `int32_t (*)(req, req_len, resp, resp_len, ctx)`，运行在**接收方**。 |

### 三种消息模型

| 操作 | API | 语义 | 是否有响应 |
|------|-----|------|-----------|
| **CALL** | `ipc_bus_call()` | 同步请求 / 应答，阻塞等待对端返回 | 有 |
| **POST** | `ipc_bus_post()` | 异步单向投递，发出即返回 | 无 |
| **PUBLISH** | `ipc_bus_publish()` | 一对多广播，本地 + 对端所有订阅者都收到 | 无 |

### 两种处理上下文（接收方注册时选择）

| 上下文 | 含义 | 适用场景 |
|--------|------|----------|
| `IPC_BUS_CONTEXT_DIRECT` | handler **当场**在 IPC 分发上下文里执行 | 处理简短、**不阻塞** |
| `IPC_BUS_CONTEXT_QUEUE` | 总线把请求打包投递到你的 **worker 队列**，由 worker 任务执行 | handler 可能**阻塞**或耗时 |

> CALL + QUEUE 模式下，发送方仍会阻塞等待，直到 worker 处理完（最长 `call_timeout_ms`）。

---

## 2. 快速开始

一次完整通信需要：**接收方**注册服务与 handler，**发送方**初始化后调用 `call/post/publish`。

> **ID 定义约定**（两个命名空间分开放）：
> - **service_id**：全局唯一，统一登记在 [`include/ipc/ipc_bus_services.h`](../../../include/ipc/ipc_bus_services.h) 的 `ipc_bus_service_id_t` 里，前缀 `IPC_SVC_`。
> - **msg_id / 主题**：服务内局部，定义在各服务自己的模块里。CALL/POST 用 `IPC_<服务>_MSG_*`，PUBLISH 用 `IPC_<服务>_EVT_*`（独立编号空间）。
> - 一律用 `enum`、从 1 起编号（`service_id`/`msg_id` 的 0 视为非法）。

### 2.1 接收方（提供服务的核）

```c
/* include/ipc/ipc_bus_services.h —— 登记全局服务号 */
typedef enum
{
    IPC_SVC_FOO = 1,
} ipc_bus_service_id_t;
```

```c
#include "ipc_bus.h"
#include "ipc_bus_services.h"

/* 本服务的版本与消息号，就近定义在自己的模块里 */
#define IPC_SVC_FOO_VERSION   1

typedef enum { IPC_FOO_MSG_PING = 1 } ipc_foo_msg_t;   /* CALL / POST */

/* handler 运行在接收方：解析 req，按需写 resp */
static int32_t ping_handler(const void *req_buf, uint32_t req_len,
                            void *resp_buf, uint32_t *resp_len, void *ctx)
{
    const uint32_t *in  = req_buf;
    uint32_t       *out = resp_buf;

    /* 校验入参与响应缓冲容量 */
    if (req_len != sizeof(uint32_t) || resp_len == NULL || *resp_len < sizeof(uint32_t))
        return LS_FAIL;

    *out = *in + 1;
    *resp_len = sizeof(uint32_t);   /* 写回实际响应长度 */
    return LS_OK;
}

void foo_service_init(void)
{
    ipc_bus_register_service(IPC_SVC_FOO, IPC_SVC_FOO_VERSION);

    /* DIRECT 模式：cfg 传 NULL 即用默认（直接执行） */
    ipc_bus_register_msg_handler(IPC_SVC_FOO, IPC_FOO_MSG_PING, ping_handler, NULL, NULL);
}
```

### 2.2 发送方（调用服务的核）

```c
#include "ipc_bus.h"

void my_client_demo(void)
{
    uint32_t req = 100;
    uint32_t resp = 0;
    uint32_t resp_len = sizeof(resp);   /* 入参=缓冲容量，出参=实际长度 */

    if (ipc_bus_call(IPC_SVC_FOO, IPC_FOO_MSG_PING,
                     &req, sizeof(req),
                     &resp, &resp_len,
                     IPC_BUS_DEFAULT_TIMEOUT_MS) == LS_OK)
    {
        /* resp == 101 */
    }
}
```

> ⚠️ `ipc_bus_call` 的 `resp_len` 是**入/出参**：调用前置为响应缓冲容量，成功后被改写为实际响应长度；失败时被清 0。

---

## 3. QUEUE 模式（处理函数需要阻塞 / 耗时）

当 handler 不适合在 IPC 分发上下文里跑（要阻塞、要做重活），用 QUEUE 模式：总线把消息投到你的队列，你在自己的任务里取出并执行。

```c
static rtos_queue       g_queue;
static rtos_task_handle g_task;

/* worker 任务：从队列取 ipc_bus_msg_t*，交回总线执行 */
static RTOS_TASK_FCT(my_worker)
{
    rtos_queue queue = env;
    ipc_bus_msg_t *msg;

    while (1)
    {
        if (rtos_queue_read(queue, &msg, -1, false) == 0)
            ipc_bus_msg_handle(msg);    /* 运行真正的 handler，并完成应答/释放 */
    }
}

void my_queued_service_init(void)
{
    ipc_bus_handler_cfg_t cfg = {0};

    /* 队列元素就是一个指针：sizeof(ipc_bus_msg_t *) */
    rtos_queue_create(sizeof(ipc_bus_msg_t *), 4, &g_queue);
    rtos_task_create(my_worker, "ipc_worker", APPLICATION_TASK,
                     256, g_queue, RTOS_TASK_PRIORITY(7), &g_task);

    cfg.context            = IPC_BUS_CONTEXT_QUEUE;
    cfg.queue              = g_queue;
    cfg.enqueue_timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;  /* 入队等待上限 */
    cfg.call_timeout_ms    = IPC_BUS_DEFAULT_TIMEOUT_MS;  /* CALL 等 worker 上限 */

    ipc_bus_register_service(IPC_SVC_FOO, IPC_SVC_FOO_VERSION);
    ipc_bus_register_msg_handler(IPC_SVC_FOO, IPC_FOO_MSG_PING, ping_handler, &cfg, NULL);
}
```

QUEUE 模式要点：
- `ipc_bus_msg_t` 是**不透明类型**，worker 只当指针传递，**不要访问其字段**。
- worker 取出消息后**必须**调用 `ipc_bus_msg_handle(msg)`（执行 handler + 完成应答 + 释放）；
  若决定丢弃，则调用 `ipc_bus_msg_release(msg)`（CALL 发送方会收到 `LS_FAIL`）。
- 二者都会把消息所有权交回总线，由总线释放，**worker 不要自己 free**。

### ⚠️ QUEUE + CALL 的队头阻塞（Head-of-line blocking）

对端来的总线请求统一由**一个** IPC Bus MRPC 服务任务串行分发。当一个 QUEUE 模式的 **CALL** 到达时，分发流程会**阻塞这个服务任务**，直到对应 worker 处理完该消息（最长 `call_timeout_ms`）才返回。

这意味着在此期间：
- **本核所有其他总线请求**（无论 CALL/POST/PUBLISH、无论哪个服务）都排在后面，无法被分发；
- 实际吞吐被最慢的那个 QUEUE 模式 CALL handler 拖住——这正是「队头阻塞」。

规避建议：
- QUEUE 模式 handler 应尽量缩短处理时间；明显耗时的逻辑进一步拆分，避免长时间占用 worker。
- 给 QUEUE 模式 CALL 设置**合理且偏小**的 `call_timeout_ms`，避免异常时长时间堵住服务任务。
- 对「只投递、不需要应答」的耗时操作优先用 **POST**（异步），而不是 CALL——POST 入队即返回，不会阻塞分发任务等待 worker。
- 若某服务确实需要长耗时的同步 CALL，考虑让它独占一个 worker 队列/任务，并在设计上接受期间总线串行化的事实。

> 提示：DIRECT 模式的 CALL 同样在分发上下文里同步执行，handler 一旦阻塞会直接卡住分发任务——所以耗时/阻塞逻辑无论如何都不该放进 DIRECT。

---

## 4. PUBLISH 订阅 / 广播

同一 `service_id + msg_id` 可注册**多个** publish handler（DIRECT/QUEUE 皆可）。`ipc_bus_publish()` 会先派发给本地所有订阅者，再发送给对端。

```c
/* PUBLISH 主题：独立编号空间，前缀 EVT 区别于 MSG */
typedef enum { IPC_FOO_EVT_NOTICE = 1 } ipc_foo_evt_t;

/* 订阅端 */
ipc_bus_register_service(IPC_SVC_FOO, IPC_SVC_FOO_VERSION);
ipc_bus_register_publish_handler(IPC_SVC_FOO, IPC_FOO_EVT_NOTICE, notice_handler, NULL, ctx_a);
ipc_bus_register_publish_handler(IPC_SVC_FOO, IPC_FOO_EVT_NOTICE, notice_handler, NULL, ctx_b);

/* 发布端 */
struct notice n = { .value = 42 };
ipc_bus_publish(IPC_SVC_FOO, IPC_FOO_EVT_NOTICE, &n, sizeof(n));
```

> 取消订阅需传入注册时**相同的 handler 与 ctx**：`ipc_bus_unregister_publish_handler(...)`。

---

## 5. API 速查

| 函数 | 作用 |
|------|------|
| `ipc_bus_register_service(id, ver)` | 注册/更新服务（必须先于 handler） |
| `ipc_bus_unregister_service(id)` | 注销服务及其下所有 handler |
| `ipc_bus_register_msg_handler(id, msg, h, cfg, ctx)` | 注册 CALL/POST 处理函数（同一 id+msg 唯一） |
| `ipc_bus_unregister_msg_handler(id, msg)` | 注销 CALL/POST 处理函数 |
| `ipc_bus_register_publish_handler(id, msg, h, cfg, ctx)` | 订阅广播（同一 id+msg 可多个） |
| `ipc_bus_unregister_publish_handler(id, msg, h, ctx)` | 取消一个订阅 |
| `ipc_bus_call(...)` | 同步请求/应答 |
| `ipc_bus_post(...)` | 异步单向投递 |
| `ipc_bus_publish(...)` | 广播 |
| `ipc_bus_get_max_payload()` | 单包最大 payload 字节数（已扣除协议头） |
| `ipc_bus_msg_handle(msg)` / `ipc_bus_msg_release(msg)` | QUEUE worker 执行/丢弃消息 |

### 容量上限（编译期可覆盖）

总线用静态数组管理注册项，默认上限较小，按需在编译选项里覆盖：

| 宏 | 默认 | 含义 |
|----|------|------|
| `IPC_BUS_MAX_SERVICES` | 2 | 同时注册的服务数 |
| `IPC_BUS_MAX_MSG_HANDLERS` | 4 | CALL/POST 处理函数槽位 |
| `IPC_BUS_MAX_PUB_HANDLERS` | 4 | 订阅槽位 |

超出上限时注册返回 `LS_ERR_NO_MEM`。

---

## 6. 常见错误（务必检查返回值）

| 现象 | 原因 |
|------|------|
| 注册返回 `LS_ERR_NOT_FOUND` | 注册 handler 前没先 `ipc_bus_register_service()` |
| 注册返回 `LS_ERR_NO_MEM` | 超出 `IPC_BUS_MAX_*` 上限，调大对应宏 |
| `ipc_bus_call` 返回 `LS_ERR_VERSION` | 收发两端 version 不一致（且都非 `*_ANY`） |
| CALL 失败、`resp` 为空 | handler 未写 `*resp_len`，或写入超过发送方提供的容量 |
| `req_len > ipc_bus_get_max_payload()` 直接失败 | payload 超过单包上限 |
| DIRECT handler 里阻塞导致卡死 | 阻塞/耗时操作必须改用 QUEUE 模式 |

---

## 7. 运行本目录 demo

`ipc_bus_demo.c` 在 `CFG_AMP_IPC_BUS_DEMO` 下提供完整范例：

- `ipc_bus_demo_server_init()` —— 注册 demo 服务：PING（DIRECT/CALL）、NOTIFY（QUEUE/POST）、NOTICE（DIRECT/PUBLISH）。
- `ipc_bus_demo_ping(v)` —— 发起一次 CALL，返回 `v+1`。
- `ipc_bus_demo_post(v)` —— 发起一次 POST。
- `ipc_bus_demo_publish(v)` —— 发起一次 PUBLISH。

典型用法：提供服务的核启动时调用 `ipc_bus_demo_server_init()`；另一核调用 `ipc_bus_demo_ping/post/publish()`。
