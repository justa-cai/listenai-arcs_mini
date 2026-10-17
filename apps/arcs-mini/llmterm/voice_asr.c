/* voice_asr —— 按住功能键说话 → 云端 ASR → 文本进终端自动提交 实现
 *
 * 线程模型：
 *   asr.ptt    ：20ms 轮询 PB4 电平驱动状态机；全部网络/音频调用
 *                （recognition_start/stop、record_start/stop）收敛于此
 *                （session_voice_start 可阻塞至 3s，不能进 UI 线程）。
 *   audio_dispatch（驱动线程）：音频回调只做 feed 闸门检查 + 左声道
 *                抽取 + 流缓冲写入（微秒级）。
 *   voice.ebus ：IAT 事件回调只拷文本 / line_push，不碰 LVGL/音频。
 *
 * 状态机：
 *   IDLE →(按下+gating通过)→ RECORDING →(松开/满5s)→ WAIT_IAT →(IAT_END/
 *   超时)→ IDLE；gating 未通过 → HELD_INERT（等释放，不采集）；
 *   5s 截断后按钮仍按住 → AUTO_STOPPED_HELD（忽略直到释放）。
 */
#define TAG "voice.asr"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "Driver_GPDMA.h"
#include "lisa_device.h"
#include "lisa_audio.h"
#include "lisa_gpio.h"
#include "lisa_thread.h"
#include "lisa_log.h"

#include "pinmux.h"                        /* POWER_KEY_PIN */

#include "voice_cloud.h"
#include "lsc_session_voice.h"
#include "voice_msg.h"
#include "llm_client.h"
#include "net_kb.h"
#include "term_ui.h"
#include "tts_speak.h"
#include "voice_asr.h"

#define ASR_POLL_MS        20
#define ASR_DEBOUNCE_N     3               /* 连续 3 次同电平才认变化 */
#define ASR_PTT_MAX_MS     5000            /* 录音上限，自动截断 */
#define ASR_WAIT_RESULT_MS 6000             /* IAT 结果兜底超时 */
#define ASR_REPRESS_GAP_MS 300             /* 两次按压最小间隔 */
#define ASR_TEXT_MAX       96               /* = TERM_INPUT_MAX */
#define ASR_MONO_SAMPLES   256              /* 512B 帧，与驱动缓冲同规格 */

enum {
    ASR_IDLE = 0,
    ASR_HELD_INERT,                        /* 离线/busy 按住：不采集等释放 */
    ASR_RECORDING,
    ASR_WAIT_IAT,
    ASR_AUTO_STOPPED_HELD,                 /* 5s 截断后仍按住：等释放 */
};

static struct {
    uint8_t inited;
    uint8_t enabled;
    volatile uint8_t feed;                 /* 音频回调闸门（dispatch 线程读） */
    uint8_t state;
    uint8_t deb_cnt;
    uint8_t deb_level;                     /* 去抖后的电平：1=按下 */
    uint32_t gen;                          /* 会话代数（ebus 回调比对） */
    volatile uint32_t wait_gen;
    char result[ASR_TEXT_MAX + 1];
    volatile uint16_t result_len;
    TickType_t t_press;
    TickType_t t_deadline;
    TickType_t t_last_idle;
    lisa_device_t *audio;
    lisa_device_t *gpio;
    uint8_t audio_inited;
    uint32_t press_cnt, feed_frames, result_cnt;
    uint32_t timeout_cnt, drop_q_cnt, offline_cnt;
} S;

/* ------------------------------------------------ 音频回调（dispatch 线程） */
static void asr_audio_cb(const lisa_audio_event_t *e, void *ud)
{
    (void)ud;
    if (!S.feed || !e->record_buffer || e->record_samples == 0) return;

    /* STEREO 交错采样，ch0=mic（ch1 为回采参考）；抽左声道成单声道 */
    static int16_t mono[ASR_MONO_SAMPLES];
    uint32_t n = e->record_samples / 2;    /* 帧内交错对数 */
    if (n > ASR_MONO_SAMPLES) n = ASR_MONO_SAMPLES;
    const int16_t *st = (const int16_t *)e->record_buffer;
    for (uint32_t i = 0; i < n; i++) mono[i] = st[i * 2];

    S.feed_frames++;
    voice_cloud_chat_send_audio((uint8_t *)mono, n * 2);   /* 0 超时入流缓冲 */
}

/* 首次有效按下时初始化一次（配置/回调注册），失败退化提示不轰炸 */
static int lazy_audio_init(void)
{
    int ret;
    if (S.audio_inited) return 0;

    GPDMA_Initialize();
    S.audio = lisa_device_get("audio0");
    if (!S.audio) {
        LISA_LOGE(TAG, "audio0 not found");
        return -1;
    }
    ret = lisa_audio_register_callback(S.audio, asr_audio_cb, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "audio cb register failed: %d", ret);
        return -1;
    }

    /* 16K/STEREO/16bit，差分输入 + HPF，增益对齐量产唤醒配置 */
    lisa_audio_record_config_t cfg = {
        .format = {
            .sample_rate = LISA_AUDIO_RATE_16K,
            .channels = LISA_AUDIO_CH_STEREO,
            .sample_bits = LISA_AUDIO_BIT_16,
        },
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
        .gain_l = {
            .analog_gain = CONFIG_AUDIO_DEFAULT_MIC_GAIN_A_DB,
            .digital_gain = CONFIG_AUDIO_DEFAULT_MIC_GAIN_D_DB,
        },
        .gain_r = {
            .analog_gain = CONFIG_AUDIO_DEFAULT_REF_GAIN_A_DB,
            .digital_gain = CONFIG_AUDIO_DEFAULT_REF_GAIN_D_DB,
        },
#else
        .gain = {
            .analog_gain = CONFIG_AUDIO_DEFAULT_MIC_GAIN_A_DB,
            .digital_gain = CONFIG_AUDIO_DEFAULT_MIC_GAIN_D_DB,
        },
#endif
        .differential_input = true,
        .enable_hpf = true,
    };
    ret = lisa_audio_record_config(S.audio, &cfg);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "record config failed: %d", ret);
        return -1;
    }
    S.audio_inited = 1;
    return 0;
}

/* ------------------------------------------------ 按下/停止序列（asr.ptt） */
static void ptt_begin(void)
{
    S.press_cnt++;

    if (!voice_cloud_is_connected()) {
        S.offline_cnt++;
        term_ui_sys_hint("(asr: cloud offline)");
        S.state = ASR_HELD_INERT;
        return;
    }
    if (voice_cloud_is_chat_quota_exhausted()) {
        S.offline_cnt++;
        term_ui_sys_hint("(asr: quota exhausted)");   /* 扫码充值后重连恢复 */
        S.state = ASR_HELD_INERT;
        return;
    }
    if (llm_client_busy()) {
        term_ui_sys_hint("(asr: llm busy)");
        S.state = ASR_HELD_INERT;
        return;
    }

    /* 停掉正在朗读的 TTS（无 AEC，避免拾进扬声器声） */
    tts_speak_reset();
    /* 纯识别会话：关 NLU/TTS，防 AIUI 语音回答抢答 */
    session_voice_set_nlu_tts(false, false);

    if (voice_cloud_audio_recognition_start() != 0) {
        LISA_LOGE(TAG, "recognition start failed");
        term_ui_sys_hint("(asr: start failed)");
        S.state = ASR_HELD_INERT;
        return;
    }
    if (lazy_audio_init() != 0) {
        voice_cloud_audio_recognition_stop();
        term_ui_sys_hint("(asr: mic error)");
        S.state = ASR_HELD_INERT;
        return;
    }

    S.feed = 1;
    if (lisa_audio_record_start(S.audio) != LISA_DEVICE_OK) {
        S.feed = 0;
        voice_cloud_audio_recognition_stop();
        term_ui_sys_hint("(asr: mic error)");
        S.state = ASR_HELD_INERT;
        return;
    }

    term_ui_sys_hint("(listening...)");
    S.t_press = xTaskGetTickCount();
    S.state = ASR_RECORDING;
}

static void ptt_finish(void)
{
    S.feed = 0;
    lisa_audio_record_stop(S.audio);
    voice_cloud_audio_recognition_stop();

    S.gen++;
    S.wait_gen = S.gen;
    S.result_len = 0;
    S.t_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(ASR_WAIT_RESULT_MS);
    S.state = ASR_WAIT_IAT;
}

/* ------------------------------------------------ IAT 事件（voice.ebus 线程） */
static void trim_submit(char *txt, uint16_t n)
{
    uint16_t s = 0;
    while (s < n && isspace((unsigned char)txt[s])) s++;
    while (n > s && (isspace((unsigned char)txt[n - 1]) ||
                     strchr(".,!?;:，。！？", (unsigned char)txt[n - 1]))) n--;
    txt[n] = '\0';

    if (n == s) {
        term_ui_sys_hint("(asr: nothing recognized)");
        return;
    }
    if (net_kb_line_push(txt + s) != 0) {
        S.drop_q_cnt++;
        term_ui_sys_hint("(asr: dropped)");
    }
}

static void on_iat_msg(void *unused, uint32_t msg_id, void *data,
                       uint32_t len, void *user_data)
{
    (void)unused; (void)len; (void)user_data;
    if (S.state != ASR_WAIT_IAT) return;
    if (msg_id == VOICE_MSG_CLOUD_IAT_UPDATE) {
        if (S.gen != S.wait_gen) return;    /* 过期会话 */
        const char *txt = (const char *)data;
        if (!txt) return;
        uint16_t n = strlen(txt);
        if (n > ASR_TEXT_MAX) n = ASR_TEXT_MAX;
        memcpy(S.result, txt, n);
        S.result[n] = '\0';
        S.result_len = n;
    } else if (msg_id == VOICE_MSG_CLOUD_IAT_END) {
        if (S.gen != S.wait_gen) return;
        S.result_cnt++;
        trim_submit(S.result, S.result_len);
        S.state = ASR_IDLE;
        S.t_last_idle = xTaskGetTickCount();
    }
    /* VOICE_MSG_CLOUD_SESSION_FINISHED 不作完成信号（tts_synth 也发它） */
}

/* ------------------------------------------------ asr.ptt 主循环 */
static uint8_t poll_button(void)
{
    int v = lisa_gpio_read_pin(S.gpio, POWER_KEY_PIN);
    uint8_t level = (v == LISA_GPIO_LOW) ? 1 : 0;   /* active-low */
    static uint8_t raw = 0, cnt = 0;
    if (level != raw) {
        raw = level;
        cnt = 1;
    } else if (cnt && cnt < ASR_DEBOUNCE_N) {
        cnt++;
        if (cnt >= ASR_DEBOUNCE_N) S.deb_level = raw;
    }
    return S.deb_level;
}

static void ptt_thread(void *arg)
{
    (void)arg;
    while (1) {
        uint8_t pressed = poll_button();

        switch (S.state) {
        case ASR_IDLE:
            if (pressed) {
                if (!S.enabled) { S.state = ASR_HELD_INERT; break; }
                if (xTaskGetTickCount() - S.t_last_idle <
                    pdMS_TO_TICKS(ASR_REPRESS_GAP_MS)) break;
                ptt_begin();
            }
            break;

        case ASR_HELD_INERT:
        case ASR_AUTO_STOPPED_HELD:
            if (!pressed) {
                S.state = ASR_IDLE;
                S.t_last_idle = xTaskGetTickCount();
            }
            break;

        case ASR_RECORDING:
            if (!pressed) {
                ptt_finish();
            } else if (xTaskGetTickCount() - S.t_press >=
                       pdMS_TO_TICKS(ASR_PTT_MAX_MS)) {
                ptt_finish();              /* 5s 自动截断 */
                S.state = ASR_AUTO_STOPPED_HELD;
                term_ui_sys_hint("(asr: max 5s, cut)");
            }
            break;

        case ASR_WAIT_IAT:
            if (xTaskGetTickCount() >= S.t_deadline) {
                voice_cloud_chat_stop();   /* 兜底清理会话 */
                S.timeout_cnt++;
                term_ui_sys_hint("(asr: timeout)");
                S.state = ASR_IDLE;
                S.t_last_idle = xTaskGetTickCount();
            }
            break;

        default:
            S.state = ASR_IDLE;
            break;
        }

        lisa_thread_mdelay(ASR_POLL_MS);
    }
}

/* ------------------------------------------------ 对外接口 */
int voice_asr_init(void)
{
    if (S.inited) return 0;

    /* 与 power_manager 同源：PB4，设备名走 Kconfig */
    S.gpio = lisa_device_get(CONFIG_POWER_MANAGER_POWER_KEY_GPIO_PAD);
    if (!S.gpio) {
        LISA_LOGE(TAG, "gpio %s not found", CONFIG_POWER_MANAGER_POWER_KEY_GPIO_PAD);
        return -1;
    }

    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, on_iat_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_END, on_iat_msg, NULL);

    lisa_thread_attr_t att = {
        .name = "asr.ptt",
        .stack_size = 4096,
        .priority = LISA_OS_PRIORITY_NORMAL,
    };
    if (lisa_thread_create(&att, ptt_thread, NULL) == NULL) {
        LISA_LOGE(TAG, "thread create failed");
        return -1;
    }

    S.enabled = 1;
    S.state = ASR_IDLE;
    S.t_last_idle = xTaskGetTickCount();
    S.inited = 1;
    LISA_LOGI(TAG, "push-to-talk asr ready (max %ums)", ASR_PTT_MAX_MS);
    return 0;
}

void voice_asr_set_enable(uint8_t on)
{
    S.enabled = on ? 1 : 0;
    if (!on && S.state == ASR_RECORDING) {
        ptt_finish();                      /* 关闭时收尾在途会话 */
    }
}

uint8_t voice_asr_enabled(void)
{
    return S.enabled;
}

uint8_t voice_asr_cloud_ready(void)
{
    return voice_cloud_is_connected();
}

const char *voice_asr_state_str(void)
{
    switch (S.state) {
    case ASR_HELD_INERT: return "held";
    case ASR_RECORDING: return "rec";
    case ASR_WAIT_IAT: return "wait";
    default: return "idle";
    }
}

void voice_asr_stat_get(uint32_t *press, uint32_t *frames,
                        uint32_t *results, uint32_t *timeouts,
                        uint32_t *drops, uint32_t *offline)
{
    if (press) *press = S.press_cnt;
    if (frames) *frames = S.feed_frames;
    if (results) *results = S.result_cnt;
    if (timeouts) *timeouts = S.timeout_cnt;
    if (drops) *drops = S.drop_q_cnt;
    if (offline) *offline = S.offline_cnt;
}
