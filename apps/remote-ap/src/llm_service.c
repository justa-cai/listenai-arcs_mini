// LLM 服务层：模型嵌入固件，上电自动加载，IPC 邮箱协议交互。
//
// 资源（model.bin / tok.bin）通过 .incbin 编入固件二进制。
// 长文本（问题/答案）走 LUNA 共享内存邮箱（llm_mailbox.h），
// ic_message 只发 ≤4B 状态通知（其载荷上限 10B）。

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

#include "ic_message.h"
#include "cache.h"
#include "lisa_log.h"

#include "neo.h"
#include "story_arena.h"
#include "story_llm.h"
#include "story_log.h"
#include "story_mem.h"
#include "llm_mailbox.h"

/* 嵌入的模型资源（res/model.S 定义） */
extern const uint8_t llm_model_start[];
extern const uint8_t llm_model_end[];
extern const uint8_t llm_tok_start[];
extern const uint8_t llm_tok_end[];

#define TAG "llm.svc"

#define LLM_SERVICE_ID  IC_MESSAGE_ID_LLM

/* mailbox 跨核共享内存 cache 维护（当前 AP 未开 DCACHE，为 no-op；
 * 若未来开启需保证写后 Flush / 读前 Invalidate，与 CP 侧 llm.c 对称） */
static void mb_flush(void)
{
    HAL_FlushDCache_by_Addr((uint32_t *)LLM_MAILBOX_ADDR, sizeof(llm_mailbox_t));
}
static void mb_invalidate(void)
{
    HAL_InvalidateDCache_by_Addr((uint32_t *)LLM_MAILBOX_ADDR, sizeof(llm_mailbox_t));
}

/* arena 缓冲单独声明（放 PSRAM 避免 SRAM 60KB 窗口溢出） */
static uint8_t fast_buf[64 * 1024]  __attribute__((aligned(16), section(".psram.noinit")));
static uint8_t bulk_buf[448 * 1024] __attribute__((aligned(16), section(".psram.noinit")));

static struct {
    llm_t llm;
    story_arena_t fast, bulk;
    bool ready;
    TaskHandle_t task;
} S;

static void do_generate(void);   /* 自检与 IPC 循环共用 */

/* 流式 piece 回调：每 token 追加到邮箱并通知 CP */
static uint32_t s_stream_offset = 0;

static bool llm_stream_piece(void *user, const char *piece, int len)
{
    (void)user;
    if (s_stream_offset + len < LLM_MB_TEXT_MAX - 1) {
        memcpy((void *)(LLM_MB->text + s_stream_offset), piece, len);
        s_stream_offset += len;
        LLM_MB->text[s_stream_offset] = '\0';
        LLM_MB->length = s_stream_offset;
        mb_flush();
        llm_notify_t n = { .state = LLM_MB_PIECE };
        ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_IPC_TYPE_EVT,
                                  &n, sizeof n);
    }
    return true;
}

/* -------------------------------------------------- 推理 worker */

/* ---- 启动自检（LLM_BOOT_SELFTEST）----
 * 定位问题时置 1：CRC 校验 flash 权重 + 两组对比生成
 * （S1 本地常量无回调 / S2 mailbox 全路径含流式回调），
 * 结果写入邮箱，CP 侧 `llm last` 读取。平时关闭（每组耗时 ~2.6s）。 */
#define LLM_BOOT_SELFTEST 0
#if LLM_BOOT_SELFTEST
#define LLM_CRC_MODEL_EXPECTED 0xed0758ceu   /* 主机 zlib.crc32(res/model.bin) */
#define LLM_CRC_TOK_EXPECTED   0x976ddb3cu   /* 主机 zlib.crc32(res/tok.bin)  */

static uint32_t dbg_crc32(const volatile uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (uint32_t)(-(int32_t)(c & 1)));
    }
    return ~c;
}
#endif

static void llm_service_task(void *arg)
{
    (void)arg;
    LISA_LOGI(TAG, "llm service task up");

    const size_t model_len = llm_model_end - llm_model_start;
    const size_t tok_len = llm_tok_end - llm_tok_start;

    LISA_LOGI(TAG, "loading model %u B + tok %u B from embedded flash",
              (unsigned)model_len, (unsigned)tok_len);

    story_arena_init(&S.fast, fast_buf, sizeof fast_buf);
    story_arena_init(&S.bulk, bulk_buf, sizeof bulk_buf);
    story_arena_begin(&S.fast, "llm-fast");
    story_arena_begin(&S.bulk, "llm-bulk");

    llm_blobs_t blobs = {
        .model = llm_model_start,
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

#if LLM_BOOT_SELFTEST
    {
        /* 三角定位自检，结果进邮箱（AP 日志走 UART，adb 看不到）：
         *  S1 = 本地常量 + 无流式回调（基线，引擎正确性）
         *  S2 = 问题经 mailbox + 含流式回调（与 IPC 请求完全同路径）
         *  Q  = 生成入口读到的问题快照（AP 实际编码的内容）
         * S1 对 S2 错 → 流式回调或 mailbox 读路径；Q 乱 → mailbox 存储被覆盖 */
        uint32_t mc = dbg_crc32(llm_model_start, model_len);
        uint32_t tc = dbg_crc32(llm_tok_start, tok_len);
        static char ans1[LLM_ANSWER_CHARS + 1], ans2[LLM_ANSWER_CHARS + 1];
        llm_stats_t st1, st2;

        llm_answer(&S.llm, NULL, "what is one plus one",
                   ans1, sizeof ans1, NULL, NULL, &st1);

        /* S2：问题写入 mailbox（模拟 CP 写入），走与 IPC 请求完全相同的路径 */
        strcpy((char *)LLM_MB->text, "what is one plus one");
        LLM_MB->state = LLM_MB_QUESTION;
        do_generate();
        strncpy(ans2, (const char *)LLM_MB->text, sizeof ans2 - 1);
        ans2[sizeof ans2 - 1] = '\0';
        st2.gen_tokens = LLM_MB->tokens;
        st2.gen_us = (int64_t)LLM_MB->ms * 1000;

        int n = snprintf((char *)LLM_MB->text, LLM_MB_TEXT_MAX,
                         "crc_model=%08x(%s) crc_tok=%08x(%s) "
                         "S1[local,no-cb]:%s | S2[mb+stream,%utok]:%s",
                         mc, mc == LLM_CRC_MODEL_EXPECTED ? "OK" : "BAD",
                         tc, tc == LLM_CRC_TOK_EXPECTED ? "OK" : "BAD",
                         ans1, (unsigned)st2.gen_tokens, ans2);
        LLM_MB->length = n > 0 ? (uint32_t)n : 0;
        LLM_MB->tokens = (uint32_t)st1.gen_tokens;
        LLM_MB->ms = (uint32_t)(st1.gen_us / 1000);
        __sync_synchronize();
        LLM_MB->state = LLM_MB_DONE;
        mb_flush();
        llm_notify_t n2 = { .state = LLM_MB_DONE };
        ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_IPC_TYPE_EVT, &n2, sizeof n2);
    }
#endif

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        mb_invalidate();
        if (LLM_MB->state != LLM_MB_QUESTION) continue;
        if (!LLM_MB->text[0]) continue;

        do_generate();
    }
}

/* 从 mailbox 取问题并生成；DONE 载荷带 Q-echo（AP 实际编码的问题快照） */
static void do_generate(void)
{
    char qsnap[LLM_MB_TEXT_MAX];
    strncpy(qsnap, (const char *)LLM_MB->text, sizeof qsnap - 1);
    qsnap[sizeof qsnap - 1] = '\0';

    LISA_LOGI(TAG, "generating: '%s'", qsnap);

    /* 清空邮箱文本区，准备流式写入 */
    LLM_MB->length = 0;
    LLM_MB->tokens = 0;
    LLM_MB->ms = 0;
    LLM_MB->text[0] = '\0';
    s_stream_offset = 0;

    static char answer[LLM_ANSWER_CHARS + 1];
    llm_stats_t st;
    bool ok = llm_answer(&S.llm, NULL, qsnap,
                         answer, sizeof answer, llm_stream_piece, NULL, &st);

    /* 写入最终统计 */
    LLM_MB->tokens = (uint32_t)st.gen_tokens;
    LLM_MB->ms = (uint32_t)(st.gen_us / 1000);

    if (!ok) {
        LISA_LOGE(TAG, "generate failed");
        LLM_MB->length = 0;
    } else {
        /* Q-echo：把 AP 实际读到的问题前缀写回载荷，CP 侧可见 */
        uint32_t alen = LLM_MB->length;
        uint32_t qroom = LLM_MB_TEXT_MAX - alen - 1;
        if (alen + 16 < LLM_MB_TEXT_MAX) {
            uint32_t ql = strlen(qsnap);
            if (ql > qroom - 12) ql = qroom - 12;
            memmove((void *)(LLM_MB->text + alen), " |Q=[", 5);
            memcpy((void *)(LLM_MB->text + alen + 5), qsnap, ql);
            memcpy((void *)(LLM_MB->text + alen + 5 + ql), "]", 2);
            LLM_MB->length = alen + 5 + ql + 1;
            mb_flush();
        }
        LISA_LOGI(TAG, "done(%ums, %u tok): %s",
                  LLM_MB->ms, LLM_MB->tokens, LLM_MB->text);
    }

    __sync_synchronize();
    LLM_MB->state = LLM_MB_DONE;
    mb_flush();

    llm_notify_t n = { .state = LLM_MB_DONE };
    int ret = ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_IPC_TYPE_EVT,
                                        &n, sizeof n);
    LISA_LOGI(TAG, "notify DONE ret=%d", ret);
}

/* -------------------------------------------------- IPC 回调 */
static int32_t llm_ipc_callback(ic_message_handle_info_t *h,
                                ic_message_msg_info_t *m)
{
    (void)h;
    if (!m || !m->msg || m->len < 4) return -1;

    /* 注意：ic_message 接收侧 msg_type 硬编码为 CMD（线上不传类型），
     * 不能用 msg_type 区分方向，只能按 state 分发。
     * QUESTION 只可能来自 CP。 */
    llm_notify_t *n = m->msg;
    if (n->state == LLM_MB_QUESTION && S.ready) {
        if (S.task) xTaskNotifyGive(S.task);
    }
    return 0;
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

    int ret = ic_message_register_by_id(LLM_SERVICE_ID, llm_ipc_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "ic_message register failed: %d", ret);
        return -1;
    }

    /* 清零邮箱 */
    LLM_MB->state = LLM_MB_IDLE;
    LLM_MB->text[0] = '\0';
    mb_flush();

    LISA_LOGI(TAG, "llm service initialized (service_id=0x%x)", LLM_SERVICE_ID);
    return 0;
}
