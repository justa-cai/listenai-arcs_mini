/* tts_speak —— LLM 答案逐句云端 TTS 朗读 实现
 *
 * 数据流：llm_client PIECE/DONE（rpc_client 线程）→ 句子切分（纯内存）
 *   → tts_q 队列 → "tts.speak" 任务（LOW 优先级）→ voice_cloud_tts_synth()
 *   → VOICE_MSG_CLOUD_TTS_URL → voice_player_tts 自动排队播放。
 *
 * 线程约束：订阅回调在 rpc_client（栈 2048）只做字符累积与入队
 * （xQueueSend 0 超时）；voice_cloud_* 网络调用全部收敛在 tts.speak
 * 任务里，reset 的 cancel 请求也经队列传递（ctl 项），不跨线程直调。
 */
#define TAG "tts.speak"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "lisa_thread.h"
#include "lisa_log.h"

#include "voice_cloud.h"
#include "voice_player/voice_player_tts.h"

#include "llm_client.h"
#include "tts_speak.h"

#define TTS_SENT_MAX   160         /* 单句容量（含 NUL），上限强切 */
#define TTS_Q_LEN      6           /* 排队句数；播放侧 tts 队列深 5 */
#define TTS_TASK_STACK 3072

typedef struct {
    uint8_t ctl;                   /* 1=取消打断控制项（text 无效） */
    char text[TTS_SENT_MAX];
} tts_item_t;

static struct {
    uint8_t inited;
    uint8_t enabled;
    uint8_t pending_dot;           /* 句点未决：等下一字符判断小数/缩写 */
    uint16_t drop_cnt;
    QueueHandle_t q;
    char sent[TTS_SENT_MAX];       /* 未闭合句累积（rpc_client 侧） */
    uint16_t slen;
} S;

/* ------------------------------------------------ 句子入队/切分（轻量） */
static void sent_flush(void)
{
    /* trim 前导空白；空句不入队 */
    uint16_t start = 0;
    while (start < S.slen && (unsigned char)S.sent[start] == ' ') start++;
    uint16_t len = S.slen - start;
    while (len > 0 && isspace((unsigned char)S.sent[start + len - 1])) len--;
    if (len == 0) { S.slen = 0; return; }

    tts_item_t it = { .ctl = 0 };
    if (len >= sizeof it.text) len = sizeof it.text - 1;
    memcpy(it.text, S.sent + start, len);
    it.text[len] = '\0';
    S.slen = 0;

    if (xQueueSend(S.q, &it, 0) != pdTRUE) {
        S.drop_cnt++;                        /* 队列满：丢本句保节奏 */
        LISA_LOGW(TAG, "queue full, drop 1 sentence (total %u)", S.drop_cnt);
    }
}

static void on_piece(const char *delta, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)delta[i];

        if (c == '.') {
            S.pending_dot = 1;               /* 暂存，看下一字符定性 */
            continue;
        }
        if (S.pending_dot) {
            S.pending_dot = 0;
            if (isalnum(c)) {
                if (S.slen >= TTS_SENT_MAX - 3) sent_flush();
                S.sent[S.slen++] = '.';      /* "3.5"/"Mr." 类：并入句内 */
            } else {
                sent_flush();                /* 真句尾 */
                if (c == ' ') continue;      /* 句间空白丢弃 */
            }
        }
        if (c == '!' || c == '?' || c == '\n') {
            sent_flush();
            continue;
        }
        if (c < 0x20 || c == 0x7f) continue; /* 其余控制字符丢弃 */

        if (S.slen >= TTS_SENT_MAX - 2) sent_flush();  /* 长句上限强切 */
        S.sent[S.slen++] = (char)c;
    }
}

/* ------------------------------------------------ LLM 订阅（rpc_client） */
static void on_llm_evt(void *user, int evt, const char *delta,
                       uint32_t len, uint32_t tokens, uint32_t ms)
{
    (void)user; (void)tokens; (void)ms;
    if (!S.inited || !S.enabled) return;

    if (evt == LLM_CLIENT_EVT_PIECE && delta && len) {
        on_piece(delta, len);
    } else if (evt == LLM_CLIENT_EVT_DONE) {
        if (delta && len) on_piece(delta, len);
        S.pending_dot = 0;
        sent_flush();                        /* 尾句 */
    } else {                                 /* LLM_CLIENT_EVT_LOST */
        S.slen = 0;                          /* 生成失败：残余不读 */
        S.pending_dot = 0;
    }
}

/* ------------------------------------------------ tts.speak 任务 */
static void speak_thread(void *arg)
{
    (void)arg;
    tts_item_t it;
    while (1) {
        if (xQueueReceive(S.q, &it, portMAX_DELAY) != pdTRUE) continue;

        if (it.ctl) {                        /* 打断：作废在途合成+停播 */
            if (voice_cloud_is_connected()) {
                voice_cloud_tts_cancel();    /* 云会话未建立时跳过，避免
                                               * session_text_cancel 报错 */
            }
            voice_player_tts_stop_async();
            continue;
        }
        if (!S.enabled) continue;            /* 关闭：排空即可 */

        if (!voice_cloud_is_connected()) {
            S.drop_cnt++;
            LISA_LOGD(TAG, "cloud offline, skip: %s", it.text);
            continue;
        }
        if (voice_cloud_tts_synth(it.text) != 0) {
            S.drop_cnt++;
            LISA_LOGW(TAG, "synth failed: %s", it.text);
        }
    }
}

/* ------------------------------------------------ 对外接口 */
int tts_speak_init(void)
{
    if (S.inited) return 0;

    S.q = xQueueCreate(TTS_Q_LEN, sizeof(tts_item_t));
    if (!S.q) {
        LISA_LOGE(TAG, "queue create failed");
        return -1;
    }

    lisa_thread_attr_t att = {
        .name = "tts.speak",
        .stack_size = TTS_TASK_STACK,
        .priority = LISA_OS_PRIORITY_LOW,    /* 低于 worq.ui/net.kb */
    };
    if (lisa_thread_create(&att, speak_thread, NULL) == NULL) {
        LISA_LOGE(TAG, "thread create failed");
        return -1;
    }

    if (llm_client_subscribe(on_llm_evt, NULL) != 0) {
        LISA_LOGE(TAG, "llm subscribe failed");
        return -1;
    }

    S.enabled = 1;
    S.inited = 1;
    LISA_LOGI(TAG, "tts speak ready (sentence %u B, queue %u)",
              (unsigned)TTS_SENT_MAX, (unsigned)TTS_Q_LEN);
    return 0;
}

void tts_speak_reset(void)
{
    if (!S.inited) return;
    S.slen = 0;
    S.pending_dot = 0;

    tts_item_t it;
    while (xQueueReceive(S.q, &it, 0) == pdTRUE) { /* 清空排队句子 */
    }
    it.ctl = 1;                              /* cancel 动作交给任务执行 */
    it.text[0] = '\0';
    if (xQueueSend(S.q, &it, 0) != pdTRUE) {
        /* 队列刚清空仍失败：极小概率，忽略 */
    }
}

void tts_speak_set_enable(uint8_t on)
{
    S.enabled = on ? 1 : 0;
    if (!on) {
        S.slen = 0;
        S.pending_dot = 0;
        tts_speak_reset();
    }
}

uint8_t tts_speak_enabled(void)
{
    return S.enabled;
}

uint8_t tts_speak_cloud_ready(void)
{
    return voice_cloud_is_connected();
}

uint32_t tts_speak_dropped(void)
{
    return S.drop_cnt;
}
