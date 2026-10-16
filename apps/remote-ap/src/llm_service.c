// LLM 服务层：模型嵌入固件，上电自动加载，IPC 监听 chat 请求。
//
// 资源（model.bin / tok.bin）通过 .incbin 编入固件二进制，
// 烧录一次即就绪，无需运行时从 flash 搬运。
//
// 协议（ic_message 自定义子命令）：
//   CP → AP  LLM_MSG_CHAT_REQ   { const char *question }
//   AP → CP  LLM_MSG_CHAT_PIECE { const char *piece }    流式
//   AP → CP  LLM_MSG_CHAT_DONE  { int gen_tokens, int64_t gen_ms }

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* newlib 软浮点可能不提供 expf 单精度版本，用 exp(double) 转发 */
__attribute__((weak)) float expf(float x)
{
    return (float)exp((double)x);
}

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "ic_message.h"
#include "lisa_log.h"

#include "neo.h"
#include "story_arena.h"
#include "story_llm.h"
#include "story_log.h"
#include "story_mem.h"

/* 嵌入的模型资源（res/model.S 定义） */
extern const uint8_t llm_model_start[];
extern const uint8_t llm_model_end[];
extern const uint8_t llm_tok_start[];
extern const uint8_t llm_tok_end[];

#define TAG "llm.svc"

/* ic_message 自定义服务 ID（避开 SDK 内置 0-29） */
#define LLM_SERVICE_ID  IC_MESSAGE_ID_LLM

/* 消息类型 */
#define LLM_MSG_CHAT_REQ    0x01
#define LLM_MSG_CHAT_RESP   0x02   /* 流式 piece */
#define LLM_MSG_CHAT_DONE   0x03   /* 最终 answer + stats */

/* arena 缓冲单独声明（section 属性不能用于结构体成员；
 * 放 PSRAM 避免 SRAM 60KB 窗口溢出） */
static uint8_t fast_buf[64 * 1024]  __attribute__((aligned(16), section(".psram.noinit")));
static uint8_t bulk_buf[448 * 1024] __attribute__((aligned(16), section(".psram.noinit")));

/* 运行时状态 */
static struct {
    llm_t llm;
    story_arena_t fast, bulk;
    bool ready;
    TaskHandle_t task;
    char question[256];
} S;

/* -------------------------------------------------- 推理 worker */
static void llm_service_task(void *arg)
{
    (void)arg;
    LISA_LOGI(TAG, "llm service task up");

    /* 启动时加载模型（从嵌入的 flash 副本整块拷入 PSRAM 保速度） */
    const size_t model_len = llm_model_end - llm_model_start;
    const size_t tok_len = llm_tok_end - llm_tok_start;

    LISA_LOGI(TAG, "loading model %u B + tok %u B from embedded flash",
              (unsigned)model_len, (unsigned)tok_len);

    story_arena_init(&S.fast, fast_buf, sizeof fast_buf);
    story_arena_init(&S.bulk, bulk_buf, sizeof bulk_buf);
    story_arena_begin(&S.fast, "llm-fast");
    story_arena_begin(&S.bulk, "llm-bulk");

    llm_blobs_t blobs = {
        .model = llm_model_start,       /* flash XIP 直接引用（16 对齐保证） */
        .model_len = model_len,
        .tok = llm_tok_start,
        .tok_len = tok_len,
    };
    llm_params_t params = llm_default_params();

    int64_t t0 = story_time_us();
    if (!llm_load(&S.llm, &blobs, &params, &S.fast, &S.bulk)) {
        LISA_LOGE(TAG, "llm_load failed — service not ready");
        vTaskDelete(NULL);
        return;
    }
    S.ready = true;
    LISA_LOGI(TAG, "model loaded in %lld ms, service ready",
              (long long)((story_time_us() - t0) / 1000));

    /* 等待 chat 请求 */
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!S.question[0]) continue;

        LISA_LOGI(TAG, "generating: '%s'", S.question);

        static char answer[LLM_ANSWER_CHARS + 1];
        llm_stats_t st;
        bool ok = llm_answer(&S.llm, NULL, S.question,
                             answer, sizeof answer,
                             NULL, NULL, &st);
        if (!ok) {
            LISA_LOGE(TAG, "generate failed");
            /* 通知 CP 失败（空 answer + done） */
            ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_MSG_CHAT_DONE,
                                      "", 1);
            S.question[0] = '\0';
            continue;
        }

        LISA_LOGI(TAG, "answer(%lldms, %d tok): %s",
                  st.gen_us / 1000, st.gen_tokens, answer);

        /* 回传完整 answer（一条消息搞定，流式后续可加 piece 回调） */
        ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_MSG_CHAT_RESP,
                                  answer, strlen(answer) + 1);
        /* done 信号带统计 */
        struct { int32_t tokens; int32_t ms; } stats = {
            st.gen_tokens, (int32_t)(st.gen_us / 1000),
        };
        ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_MSG_CHAT_DONE,
                                  &stats, sizeof stats);

        S.question[0] = '\0';
    }
}

/* -------------------------------------------------- IPC 回调 */
static int32_t llm_ipc_callback(ic_message_handle_info_t *h,
                                ic_message_msg_info_t *m)
{
    (void)h;
    if (!m || !m->msg || m->len == 0) return -1;

    switch (m->msg_type) {
    case LLM_MSG_CHAT_REQ:
        if (!S.ready) {
            LISA_LOGW(TAG, "model not loaded yet, drop chat request");
            return -1;
        }
        uint32_t len = m->len;
        if (len >= sizeof S.question) len = sizeof S.question - 1;
        memcpy(S.question, m->msg, len);
        S.question[len] = '\0';
        LISA_LOGI(TAG, "chat req: '%s'", S.question);
        /* 唤醒推理 worker */
        if (S.task) xTaskNotifyGive(S.task);
        return 0;
    default:
        return 0;
    }
}

/* -------------------------------------------------- 初始化 */
int llm_service_init(void)
{
    BaseType_t ok = xTaskCreate(llm_service_task, "llm.svc",
                                8192, NULL, 4, &S.task);
    if (ok != pdPASS) {
        LISA_LOGE(TAG, "llm service task create failed");
        return -1;
    }

    /* 注册 ic_message 服务，接收 CP 的 chat 请求 */
    int ret = ic_message_register_by_id(LLM_SERVICE_ID, llm_ipc_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "ic_message register failed: %d", ret);
        return -1;
    }

    LISA_LOGI(TAG, "llm service initialized (service_id=0x%x)", LLM_SERVICE_ID);
    return 0;
}
