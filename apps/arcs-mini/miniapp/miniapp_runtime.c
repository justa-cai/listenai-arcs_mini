#include "miniapp.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "cJSON.h"
#include "listen_system.h"
#include "miniapp_storage.h"
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include <soc/chip.h>
#include "lauxlib.h"
#include "alarm_handler.h"
#include "alarm_ring.h"
#include "app_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lua.h"
#include "lualib.h"
#include "lsc.h"
#include "lsc_errno.h"
#include "lsc_sessions_core.h"
#include "service_led.h"
#include "service_power_policy.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "voice_intent_mgr.h"
#include "voice_player/voice_player_music.h"
#include "voice_player/voice_player_tts.h"
#include "voice_player_comm.h"
#include "lisa_http.h"
#include "HTTPCUsr_api.h"

#define TAG "miniapp"
#define MINIAPP_HOOK_GRANULARITY 1000u

typedef enum {
    MINIAPP_EVENT_INSTALL,
    MINIAPP_EVENT_CLICK,
    MINIAPP_EVENT_EXIT,
} miniapp_event_type_t;

typedef struct {
    miniapp_event_type_t type;
    char button_id[MINIAPP_BUTTON_ID_MAX + 1];
    void *request;
    uint32_t generation;
} miniapp_event_t;

typedef struct {
    uint32_t speech_id;
    uint32_t generation;
    char text[MINIAPP_TTS_TEXT_MAX + 1];
    char url[512];
    bool url_ready, synth_failed, cancelled, complete;
    const char *result;
    const char *error;
} miniapp_tts_job_t;

/* One audible request, plus bounded asynchronous busy/error results. */
#define MINIAPP_TTS_RESULT_SLOTS 4u

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
    uint32_t generation;
    uint8_t kind;              /* 见下面的 MINIAPP_BUZZER_KIND_* */
} miniapp_buzzer_command_t;

/* 队列里同时跑两种活: 单音(NOTE)和"去信箱取一首整曲"(SEQ)。放同一个队列是为了
 * 只留一套唤醒/重置逻辑 —— SEQ 只是个唤醒令牌, 文本在下面的信箱里。 */
enum {
    MINIAPP_BUZZER_KIND_NOTE = 0,
    MINIAPP_BUZZER_KIND_SEQ = 1,
};

/* 一个音符: freq = 0 表示休止。 */
typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
} miniapp_note_t;

typedef struct miniapp_http_request miniapp_http_request_t;

typedef struct {
    miniapp_http_request_t *request;
    int error;
    unsigned status;
    char content_type[128];
    char error_code[24];
    char error_message[64];
    char *body;
    size_t body_len;
} miniapp_http_response_t;

struct miniapp_http_request {
    uint32_t id;
    uint32_t generation;
    bool cancelled;
    bool queued;
    char url[URL_MAX_LENGTH];
    char headers[MINIAPP_HTTP_MAX_HEADER_BYTES + 1u];
    char *body;
    size_t body_len;
    lisa_http_method_e method;
    uint32_t timeout_ms;
    size_t max_response_bytes;
    bool complete;
    miniapp_http_response_t response;
};

typedef struct {
    size_t used;
    size_t limit;
    uint32_t instruction_left;
    TickType_t deadline;
#if configGENERATE_RUN_TIME_STATS && defined(SOC_TIMER_FREQ)
    uint32_t cpu_started;
    uint32_t cpu_limit;
#endif
    bool validating;
    uint32_t http_generation;
    bool faulted, retiring, storage_read;
    char id[MINIAPP_ID_MAX + 1];
    cJSON *storage_json;
    char storage_data[MINIAPP_STORAGE_MAX_BYTES + 1];
    size_t storage_size;
    uint32_t storage_ttl;
    unsigned storage_pending; /* 0: none, 1: save, 2: clear; startup transaction */
    uint32_t present_count;
    miniapp_scene_t scene;
    miniapp_scene_t presented_scene;
    int pending_led;
    uint32_t led_on_ms, led_off_ms;
    unsigned buzz_count;
    miniapp_buzzer_command_t buzz[4];
} miniapp_vm_t;

typedef struct {
    const miniapp_package_t *package;
    const char *source;
    SemaphoreHandle_t done;
    int result;
    char error[128];
} miniapp_request_t;

static QueueHandle_t s_event_queue;
static TaskHandle_t s_task;
static QueueHandle_t s_buzzer_queue;
static TaskHandle_t s_buzzer_task;
static QueueHandle_t s_http_queue;
static TaskHandle_t s_http_task;
static SemaphoreHandle_t s_http_lock;
static miniapp_http_request_t *s_http_requests[MINIAPP_HTTP_MAX_REQUESTS];
static uint32_t s_http_next_id = 1;
static uint32_t s_http_generation;
static miniapp_status_t s_status;
static miniapp_package_t s_current;
static volatile bool s_active;
static volatile bool s_install_busy;
static volatile uint32_t s_buzzer_generation;
/* 整曲播放的信箱。文本只由 Lua 任务写、buzzer 任务读, 两边都在临界区里搬;
 * gen 每次提交 +1, buzzer 任务据此判断"手里这首是不是已经过时了"(newest wins)。
 *
 * 刻意不复用 s_buzzer_generation: 那个还被 TTS/HTTP 的异步结果校验用着, 每弹
 * 一首曲子就递增会误杀无关的待回结果。
 *
 * 也不走队列: 队列只有 4 格且发送不等待, 满的时候唤醒令牌会被丢掉, 结果是"文本
 * 更新了但没人来取"。放在队列外面还顺带免疫了 xQueueReset(xQueueReset 不会释放
 * 任何东西, 指针式传参会在每次换应用时漏一块内存)。 */
static char s_melody_text[MINIAPP_BUZZER_SEQ_MAX_BYTES];
static volatile uint32_t s_melody_gen;
/* 整曲正在响。由 Lua 侧在提交时**同步**置位 —— 放到 buzzer 任务里置位的话,
 * 从提交到任务真正开始之间排进队列的音效会先响一下、再在整曲结束后爆出来。 */
static volatile bool s_melody_playing;
static uint32_t s_install_generation;
static uint32_t s_tts_next_id = 1;
static uint32_t s_tts_worker_id;
static miniapp_tts_job_t *s_tts_requests[MINIAPP_TTS_RESULT_SLOTS];

#ifdef CONFIG_MINIAPP_ADB_DEBUG
struct miniapp_source {
    unsigned refs;
    size_t size;
    char data[];
};
static miniapp_source_t *s_source;

miniapp_source_t *miniapp_source_acquire(void)
{
    taskENTER_CRITICAL();
    miniapp_source_t *source = s_active ? s_source : NULL;
    if (source) {
        ++source->refs;
    }
    taskEXIT_CRITICAL();
    return source;
}

void miniapp_source_release(miniapp_source_t *source)
{
    if (!source) {
        return;
    }
    taskENTER_CRITICAL();
    bool last = --source->refs == 0;
    taskEXIT_CRITICAL();
    if (last) {
        lisa_mem_free(source);
    }
}

const char *miniapp_source_data(const miniapp_source_t *source, size_t *size)
{
    *size = source->size;
    return source->data;
}

static void miniapp_source_replace(miniapp_source_t *source)
{
    taskENTER_CRITICAL();
    miniapp_source_t *previous = s_source;
    s_source = source;
    taskEXIT_CRITICAL();
    miniapp_source_release(previous);
}
#endif

static void miniapp_tts_cloud_event(sessions_event_e event, void *data, uint32_t size, void *user)
{
    miniapp_tts_job_t *job = user;
    taskENTER_CRITICAL();
    if (!job->cancelled && !job->complete) {
        if (event == SESSION_TTS_URL && data && size > 1 && size <= sizeof(job->url)) {
            memcpy(job->url, data, size);
            job->url[size - 1] = 0;
            job->url_ready = true;
        } else job->synth_failed = true;
    }
    taskEXIT_CRITICAL();
}

static void miniapp_tts_worker(void *argument)
{
    miniapp_tts_job_t *job = argument;
    session_t *session = NULL;
    const char *result = "failed", *error = "unavailable";
    TickType_t started = xTaskGetTickCount();
    bool submitted = false;
    /* An independent SDK session routes late URLs by rid. Never send the
     * SDK's unscoped cancel frame: it can cancel a newer voice/alarm session. */
    session = session_create();
    if (!session) goto done;
    session_params_t config = {.data_type = "text", .tts_params = {.enable = true}};
    if (session_set_config(session, &config) != 0 ||
        session_add_evt_callback(session, miniapp_tts_cloud_event,
                                 SESSION_TTS_URL | SESSION_ERR_FRAME | SESSION_TIMEOUT, job) != 0) goto done;
    if (voice_cloud_is_session_active() || voice_cloud_is_uploading_audio() || alarm_ring_is_active()) {
        error = "busy";
        goto done;
    }
    if (voice_player_tts_owned_result(job->speech_id) != VOICE_TTS_OWNED_PENDING) {
        result = "interrupted"; error = NULL; goto done;
    }
    int start_result = session_start_text_if_idle(session, job->text);
    if (start_result != 0) {
        error = start_result == LSC_INVALID_STATE ? "busy" : "unavailable";
        goto done;
    }
    for (;;) {
        taskENTER_CRITICAL();
        bool cancelled = job->cancelled;
        bool ready = job->url_ready;
        bool failed = job->synth_failed;
        taskEXIT_CRITICAL();
        if (cancelled || alarm_ring_is_active() || voice_cloud_is_session_active() ||
            voice_cloud_is_uploading_audio()) {
            result = "interrupted"; error = NULL; break;
        }
        voice_tts_owned_result_t playback = voice_player_tts_owned_result(job->speech_id);
        if (playback != VOICE_TTS_OWNED_PENDING) {
            result = playback == VOICE_TTS_OWNED_COMPLETED ? "completed" :
                     playback == VOICE_TTS_OWNED_INTERRUPTED ? "interrupted" : "failed";
            error = playback == VOICE_TTS_OWNED_FAILED ? "unavailable" : NULL;
            break;
        }
        if (failed) break;
        if ((xTaskGetTickCount() - started) * portTICK_PERIOD_MS >= MINIAPP_TTS_TIMEOUT_MS) {
            error = "timeout"; break;
        }
        if (ready && !submitted) {
            if (!voice_player_tts_play_owned(job->speech_id, job->url)) {
                if (voice_player_tts_owned_result(job->speech_id) == VOICE_TTS_OWNED_INTERRUPTED) {
                    result = "interrupted"; error = NULL;
                }
                break;
            }
            submitted = true;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
done:
    voice_player_tts_cancel_owned(job->speech_id);
    if (session) session_destroy(session); /* waits for in-flight SDK callbacks */
    voice_player_tts_release_owned(job->speech_id);
    taskENTER_CRITICAL();
    if (s_tts_worker_id == job->speech_id) s_tts_worker_id = 0;
    job->result = result;
    job->error = error;
    job->complete = true;
    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}

static miniapp_tts_job_t *miniapp_tts_take_result(bool frozen)
{
    miniapp_tts_job_t *result = NULL;
    taskENTER_CRITICAL();
    for (unsigned i = 0; i < MINIAPP_TTS_RESULT_SLOTS; ++i) {
        miniapp_tts_job_t *job = s_tts_requests[i];
        if (job && job->complete && (!frozen || job->cancelled ||
                                    job->generation != s_buzzer_generation)) {
            result = job;
            s_tts_requests[i] = NULL;
            break;
        }
    }
    taskEXIT_CRITICAL();
    return result;
}

static void miniapp_set_active(bool active)
{
    bool changed = s_active != active;
    s_active = active;
    s_status.active = active;
    service_power_policy_set_miniapp_active(active);
    if (changed || active) {
        /* Mode and miniapp identity are captured in the start frame. Stop
         * the current interaction whenever the foreground app changes,
         * including replacement while miniapp mode is already active. The
         * next wakeup then creates a start frame with the new identity. */
        if (voice_cloud_is_session_active() || voice_cloud_is_uploading_audio()) {
            if (voice_cloud_chat_stop_local() != 0) {
                LISA_LOGW(TAG, "failed to stop voice input on miniapp mode change");
            }
        }
        if (voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0) != 0) {
            LISA_LOGW(TAG, "failed to finish voice interaction on miniapp mode change");
        }
    }
}

static void miniapp_set_error(const char *message)
{
    snprintf(s_status.last_error, sizeof(s_status.last_error), "%s",
             message ? message : "unknown error");
    LISA_LOGE(TAG, "%s", s_status.last_error);
}

static void *miniapp_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    miniapp_vm_t *vm = ud;
    if (nsize == 0) {
        if (ptr) {
            lisa_mem_free(ptr);
            vm->used = osize <= vm->used ? vm->used - osize : 0;
        }
        return NULL;
    }

    size_t old_size = ptr ? osize : 0;
    if (nsize > UINT32_MAX || vm->used > vm->limit ||
        (nsize > old_size && nsize - old_size > vm->limit - vm->used)) {
        return NULL;
    }

    void *next = ptr ? lisa_mem_realloc(ptr, (uint32_t)nsize)
                     : lisa_mem_alloc((uint32_t)nsize);
    if (next) {
        vm->used = vm->used - old_size + nsize;
    }
    return next;
}

static miniapp_vm_t *miniapp_vm_get(lua_State *L)
{
    return *(miniapp_vm_t **)lua_getextraspace(L);
}

static lua_Integer miniapp_integer(lua_State *L, int index, lua_Integer min, lua_Integer max)
{
    lua_Integer value = luaL_checkinteger(L, index);
    if (value < min || value > max) {
        luaL_error(L, "argument %d is out of range", index);
    }
    return value;
}

static int miniapp_lua_screen_begin(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    vm->scene.background = (uint32_t)luaL_checkinteger(L, 1) & 0xffffffu;
    vm->scene.rect_count = 0;
    vm->scene.text_count = 0;
    return 0;
}

static int miniapp_lua_screen_rect(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->scene.rect_count >= MINIAPP_MAX_RECTS) {
        return luaL_error(L, "screen.rect limit is %d", MINIAPP_MAX_RECTS);
    }
    miniapp_rect_t *rect = &vm->scene.rects[vm->scene.rect_count++];
    rect->x = (int16_t)miniapp_integer(L, 1, 0, MINIAPP_SCREEN_WIDTH - 1);
    rect->y = (int16_t)miniapp_integer(L, 2, 0, MINIAPP_SCREEN_HEIGHT - 1);
    rect->w = (int16_t)miniapp_integer(L, 3, 0, MINIAPP_SCREEN_WIDTH);
    rect->h = (int16_t)miniapp_integer(L, 4, 0, MINIAPP_SCREEN_HEIGHT);
    rect->color = (uint32_t)luaL_checkinteger(L, 5) & 0xffffffu;
    if (rect->w <= 0 || rect->h <= 0 || rect->x < 0 || rect->y < 0 ||
        rect->x + rect->w > MINIAPP_SCREEN_WIDTH ||
        rect->y + rect->h > MINIAPP_SCREEN_HEIGHT) {
        return luaL_error(L, "invalid rectangle");
    }
    return 0;
}

static int miniapp_lua_screen_text(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    size_t len = 0;
    const char *text = luaL_checklstring(L, 1, &len);
    if (vm->scene.text_count >= MINIAPP_MAX_TEXTS) {
        return luaL_error(L, "screen.text limit is %d", MINIAPP_MAX_TEXTS);
    }
    if (len > MINIAPP_TEXT_MAX) {
        return luaL_error(L, "text is too long");
    }
    miniapp_text_t *item = &vm->scene.texts[vm->scene.text_count++];
    item->x = (int16_t)miniapp_integer(L, 2, 0, MINIAPP_SCREEN_WIDTH - 1);
    item->y = (int16_t)miniapp_integer(L, 3, 0, MINIAPP_SCREEN_HEIGHT - MINIAPP_TEXT_FONT_PX);
    item->color = (uint32_t)luaL_checkinteger(L, 4) & 0xffffffu;
    if (item->x < 0 || item->x >= MINIAPP_SCREEN_WIDTH || item->y < 0 ||
        item->y + MINIAPP_TEXT_FONT_PX > MINIAPP_SCREEN_HEIGHT) {
        return luaL_error(L, "invalid text position");
    }
    memcpy(item->text, text, len);
    item->text[len] = '\0';
    return 0;
}

static int miniapp_lua_screen_present(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    vm->present_count++;
    vm->presented_scene = vm->scene;
    if (!vm->validating && miniapp_ui_present(&vm->scene) != 0) {
        s_status.dropped_frames++;
    }
    return 0;
}

static int miniapp_lua_led_on(lua_State *L)
{
    if (strcmp(luaL_checkstring(L, 1), "status") != 0) {
        return luaL_error(L, "unknown LED id");
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->validating) vm->pending_led = 1;
    else service_led_on();
    return 0;
}

static int miniapp_lua_led_off(lua_State *L)
{
    if (strcmp(luaL_checkstring(L, 1), "status") != 0) {
        return luaL_error(L, "unknown LED id");
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->validating) vm->pending_led = 0;
    else service_led_off();
    return 0;
}

static int miniapp_lua_led_blink(lua_State *L)
{
    if (strcmp(luaL_checkstring(L, 1), "status") != 0) {
        return luaL_error(L, "unknown LED id");
    }
    uint32_t on_ms = (uint32_t)miniapp_integer(L, 2, 10, 5000);
    uint32_t off_ms = (uint32_t)miniapp_integer(L, 3, 10, 5000);
    if (on_ms < 10 || on_ms > 5000 || off_ms < 10 || off_ms > 5000) {
        return luaL_error(L, "blink interval must be 10..5000 ms");
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->validating) {
        vm->pending_led = 2; vm->led_on_ms = on_ms; vm->led_off_ms = off_ms;
    } else service_led_blink(on_ms, off_ms);
    return 0;
}

/* 定义在下面的 buzzer 任务块里(那里才是它的使用者); Lua 绑定要先用它校验一遍,
 * 所以在这里先声明。 */
static int miniapp_buzzer_parse_notes(const char *text, miniapp_note_t *notes, unsigned max);

static void miniapp_buzzer_enqueue(const miniapp_buzzer_command_t *command)
{
    /* Sound effects are best effort. Audio contention must not fault the Lua
     * application or replay stale effects after a spoken reply or alarm. */
    if (!s_buzzer_queue) return;
    /* 整曲播放期间丢掉单音音效: 一是它们会盖在曲子上, 二是它们会在队列里排队,
     * 等整曲放完再一起爆出来。整曲自己的唤醒命令(kind=SEQ)不受这条限制 ——
     * "换一首"就是靠它。 */
    if (command->kind == MINIAPP_BUZZER_KIND_NOTE && s_melody_playing) return;
    if (voice_player_tts_is_active() || alarm_ring_is_active()) return;
    (void)xQueueSend(s_buzzer_queue, command, 0);
}

static int miniapp_lua_buzzer_play(lua_State *L)
{
    uint32_t frequency_hz = (uint32_t)miniapp_integer(L, 1, MINIAPP_BUZZER_MIN_HZ, MINIAPP_BUZZER_MAX_HZ);
    uint32_t duration_ms = (uint32_t)miniapp_integer(L, 2, MINIAPP_BUZZER_MIN_MS, MINIAPP_BUZZER_MAX_MS);
    if (frequency_hz < MINIAPP_BUZZER_MIN_HZ || frequency_hz > MINIAPP_BUZZER_MAX_HZ) {
        return luaL_error(L, "buzzer frequency must be %u..%u Hz",
                          MINIAPP_BUZZER_MIN_HZ, MINIAPP_BUZZER_MAX_HZ);
    }
    if (duration_ms < MINIAPP_BUZZER_MIN_MS || duration_ms > MINIAPP_BUZZER_MAX_MS) {
        return luaL_error(L, "buzzer duration must be %u..%u ms",
                          MINIAPP_BUZZER_MIN_MS, MINIAPP_BUZZER_MAX_MS);
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    miniapp_buzzer_command_t command = {
        .frequency_hz = (uint16_t)frequency_hz,
        .duration_ms = (uint16_t)duration_ms,
        .generation = s_buzzer_generation,
        .kind = MINIAPP_BUZZER_KIND_NOTE,
    };
    if (vm->validating) {
        if (vm->buzz_count == 4) return 0;
        vm->buzz[vm->buzz_count++] = command;
    } else {
        miniapp_buzzer_enqueue(&command);
    }
    return 0;
}

/* buzzer.play_seq(text) —— 整曲播放。text = "freq:ms,freq:ms,...", freq = 0 表示
 * 休止, 空串表示停止。后到的请求直接顶掉正在播的那首。
 *
 * 为什么要有它: 单音接口是"一个音一个 WAV", 每个音都要 stop/重建/play, 设备实测
 * 每次约 50ms 的死区, 而且是逐音累加的 —— 八分音符 250ms 被拉成 300ms、四分
 * 500ms 变成 550ms, 时值比例被改掉, 听感就是"节奏不对"。整曲走流式 PCM,
 * 音符之间没有缝。 */
static int miniapp_lua_buzzer_play_seq(lua_State *L)
{
    size_t len = 0;
    const char *text = luaL_checklstring(L, 1, &len);

    if (len >= MINIAPP_BUZZER_SEQ_MAX_BYTES) {
        return luaL_error(L, "melody is too long: %u bytes (limit %u)",
                          (unsigned)len, MINIAPP_BUZZER_SEQ_MAX_BYTES - 1u);
    }
    if (memchr(text, '\0', len) || strpbrk(text, "\r\n")) {
        return luaL_error(L, "invalid melody text");
    }
    /* 只允许数字、冒号和逗号 —— 解析器在任务侧, 这里就把字符集收窄, 免得那种
     * "看着像音谱其实混了别的字符"的文本走完全程才被丢。 */
    for (size_t i = 0; i < len; ++i) {
        char c = text[i];
        if (!isdigit((unsigned char)c) && c != ':' && c != ',') {
            return luaL_error(L, "melody may only contain digits, ':' and ','");
        }
    }

    miniapp_vm_t *vm = miniapp_vm_get(L);
    /* 启动校验期不放整曲, 也不暂存(暂存要每个 VM 多扛 3KB) —— 现有代码对第 5 个
     * 暂存单音也是静默丢弃, 语义一致。 */
    if (vm->validating || vm->faulted || vm->retiring) return 0;

    miniapp_note_t scratch[MINIAPP_BUZZER_SEQ_MAX_NOTES];
    int count = miniapp_buzzer_parse_notes(text, scratch, MINIAPP_BUZZER_SEQ_MAX_NOTES);
    if (count < 0) {
        return luaL_error(L, "malformed melody: expected 'freq:ms' pairs separated by ','");
    }

    miniapp_buzzer_command_t wake = {
        .generation = s_buzzer_generation,
        .kind = MINIAPP_BUZZER_KIND_SEQ,
    };
    taskENTER_CRITICAL();
    memcpy(s_melody_text, text, len);
    memset(s_melody_text + len, 0, sizeof(s_melody_text) - len);   /* 尾部清零, 读侧按 C 串用 */
    s_melody_gen++;                                             /* 提交后才算数 */
    s_melody_playing = (count > 0);
    taskEXIT_CRITICAL();

    /* 队列只有 4 格且发送不等待: 满的时候唤醒会丢。清空再插到队首, 顺带把已经
     * 排队的单音音效也清掉(它们本来就不该盖在新曲子前面)。 */
    if (s_buzzer_queue) {
        (void)xQueueReset(s_buzzer_queue);
        (void)xQueueSendToFront(s_buzzer_queue, &wake, 0);
    }
    LISA_LOGI(TAG, "melody: queued %d notes, %u bytes", count, (unsigned)len);
    return 0;
}

static int miniapp_lua_tts_speak(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    size_t len = 0;
    const char *text = luaL_checklstring(L, 1, &len);
    uint32_t speech_id;
    miniapp_tts_job_t *job;

    if (!len || len > MINIAPP_TTS_TEXT_MAX || memchr(text, '\0', len)) {
        return luaL_error(L, "text must be 1..%u bytes", MINIAPP_TTS_TEXT_MAX);
    }
    if (vm->validating || vm->faulted || vm->retiring) {
        return luaL_error(L, "tts unavailable during startup or exit");
    }

    job = lisa_mem_calloc(1, sizeof(*job));
    if (!job) return luaL_error(L, "not enough memory for TTS request");
    memcpy(job->text, text, len);
    job->generation = s_buzzer_generation;
    taskENTER_CRITICAL();
    unsigned slot = MINIAPP_TTS_RESULT_SLOTS;
    for (unsigned i = 0; i < MINIAPP_TTS_RESULT_SLOTS; ++i)
        if (!s_tts_requests[i]) { slot = i; break; }
    if (slot == MINIAPP_TTS_RESULT_SLOTS) {
        taskEXIT_CRITICAL();
        lisa_mem_free(job);
        return luaL_error(L, "too many pending TTS results");
    }
    speech_id = s_tts_next_id++;
    if (!speech_id) speech_id = s_tts_next_id++;
    job->speech_id = speech_id;
    s_tts_requests[slot] = job;
    bool worker_busy = s_tts_worker_id != 0;
    taskEXIT_CRITICAL();
    if (worker_busy || voice_cloud_is_session_active() || voice_cloud_is_uploading_audio() ||
        alarm_ring_is_active() || !voice_player_tts_claim(speech_id)) {
        job->result = "failed";
        job->error = "busy";
        job->complete = true;
    } else {
        taskENTER_CRITICAL();
        s_tts_worker_id = speech_id;
        taskEXIT_CRITICAL();
        if (xTaskCreate(miniapp_tts_worker, "miniapp.tts", 4096, job, 4, NULL) != pdPASS) {
            voice_player_tts_cancel_owned(speech_id);
            voice_player_tts_release_owned(speech_id);
            s_tts_worker_id = 0;
            job->result = "failed";
            job->error = "unavailable";
            job->complete = true;
        }
    }
    lua_pushinteger(L, speech_id);
    return 1;
}

static int miniapp_lua_tts_cancel(lua_State *L)
{
    lua_Integer requested = luaL_checkinteger(L, 1);
    if (requested < 1 || requested > UINT32_MAX) return luaL_error(L, "invalid speech id");
    bool accepted = false;
    taskENTER_CRITICAL();
    for (unsigned i = 0; i < MINIAPP_TTS_RESULT_SLOTS; ++i) {
        miniapp_tts_job_t *job = s_tts_requests[i];
        if (job && job->speech_id == requested && job->generation == s_buzzer_generation && !job->cancelled) {
            job->cancelled = true;
            accepted = true;
            break;
        }
    }
    taskEXIT_CRITICAL();
    if (accepted) voice_player_tts_cancel_owned((uint32_t)requested);
    lua_pushboolean(L, accepted);
    return 1;
}

#if MINIAPP_HAS_BUZZER
static void miniapp_wav_u32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = (uint8_t)(value >> (8u * i));
}

static bool miniapp_buzzer_player_active(void)
{
    app_player_state_t state = app_player_get_state(miniapp_player);
    return state == APP_PLAYER_STATE_PREPARING || state == APP_PLAYER_STATE_PREPARED ||
           state == APP_PLAYER_STATE_PLAYING || state == APP_PLAYER_STATE_PAUSED;
}

/* 把正在响的（单音或整曲）停掉。stop 在流式模式下不合法，所以失败就退到 reset
 * —— 这里出现的失败基本只有"上一个流还没收尾"这一种。 */
static void miniapp_buzzer_silence(void)
{
    if (!miniapp_player || !miniapp_buzzer_player_active()) return;
    if (app_player_stop(miniapp_player) != APP_PLAYER_OK) {
        (void)app_player_reset(miniapp_player);
    }
}

/* 解析 "freq:ms,freq:ms,..." 到音符表; 返回音数, 语法或取值不对返回 -1。
 *
 * Lua 绑定侧已经完整校验过一遍(非法直接 luaL_error 打死实例), 这里是纵深防御 ——
 * 不许 strtol, 因为要连"有没有越界、有没有多余字符"一起说清楚。只用乘加, 没有
 * 除法取模, 所以即使将来被挪进临界区也不会拖长中断关闭时间。 */
static int miniapp_buzzer_parse_notes(const char *text, miniapp_note_t *notes, unsigned max)
{
    unsigned count = 0;
    const char *p = text;
    while (*p) {
        unsigned freq = 0, ms = 0;
        if (!isdigit((unsigned char)*p)) return -1;
        while (isdigit((unsigned char)*p)) {
            freq = freq * 10u + (unsigned)(*p - '0');
            if (freq > 0xFFFFu) return -1;
            p++;
        }
        if (*p != ':') return -1;
        p++;
        if (!isdigit((unsigned char)*p)) return -1;
        while (isdigit((unsigned char)*p)) {
            ms = ms * 10u + (unsigned)(*p - '0');
            if (ms > 0xFFFFu) return -1;
            p++;
        }
        if (*p == ',') {
            p++;
        } else if (*p != '\0') {
            return -1;
        }
        if (count == max) return -1;
        if (ms < MINIAPP_BUZZER_MIN_MS || ms > MINIAPP_BUZZER_SEQ_MAX_MS) return -1;
        if (freq != 0 && (freq < MINIAPP_BUZZER_MIN_HZ || freq > MINIAPP_BUZZER_MAX_HZ)) return -1;
        notes[count].frequency_hz = (uint16_t)freq;
        notes[count].duration_ms = (uint16_t)ms;
        count++;
    }
    return (int)count;
}

/* 单音: 一个音 = 一个 WAV = 一次 app_player_play。音色是 16kHz 单声道方波。
 * 保留给 buzzer.play 用(UI 反馈音、游戏音效), 同时兼作整曲流式播放失败时的
 * 回退 —— 见下面的 miniapp_buzzer_run_melody。 */
static void miniapp_buzzer_play_wav_note(uint8_t *wav, uint16_t frequency_hz,
                                        uint16_t duration_ms, uint32_t generation)
{
    uint32_t count = (uint32_t)duration_ms * 16u;
    uint32_t bytes = count * 2u;
    memset(wav, 0, 44);
    memcpy(wav, "RIFF", 4);
    miniapp_wav_u32(wav + 4, 36u + bytes);
    memcpy(wav + 8, "WAVEfmt ", 8);
    miniapp_wav_u32(wav + 16, 16);
    wav[20] = 1; /* PCM */
    wav[22] = 1; /* mono */
    miniapp_wav_u32(wav + 24, 16000);
    miniapp_wav_u32(wav + 28, 32000);
    wav[32] = 2;
    wav[34] = 16;
    memcpy(wav + 36, "data", 4);
    miniapp_wav_u32(wav + 40, bytes);
    uint32_t phase = 0;
    for (uint32_t i = 0; i < count; ++i) {
        phase = (phase + frequency_hz) % 16000u;
        int16_t sample = phase < 8000u ? 12000 : -12000;
        wav[44u + i * 2u] = (uint8_t)sample;
        wav[45u + i * 2u] = (uint8_t)((uint16_t)sample >> 8);
    }
    char url[64];
    snprintf(url, sizeof(url), "mem://addr=%usize=%u", (unsigned)(uintptr_t)wav, bytes + 44u);
    if (app_player_play(miniapp_player, url) != APP_PLAYER_OK) {
        LISA_LOGW(TAG, "failed to play buzzer");
        return;
    }
    TickType_t started = xTaskGetTickCount();
    while (s_active && generation == s_buzzer_generation &&
           !alarm_ring_is_active() && !voice_player_tts_is_active() &&
           xTaskGetTickCount() - started < pdMS_TO_TICKS(duration_ms + 500u)) {
        app_player_state_t state = app_player_get_state(miniapp_player);
        if (state != APP_PLAYER_STATE_PREPARING && state != APP_PLAYER_STATE_PREPARED &&
            state != APP_PLAYER_STATE_PLAYING) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (miniapp_buzzer_player_active()) (void)app_player_stop(miniapp_player);
}

/* 整曲播放该不该让位: 应用退出了、被换掉了、来了一首新的、TTS 或闹钟插进来了。
 * 写 PCM 的循环每写一块查一次, 播完之后的"等尾巴"循环每 20ms 查一次。 */
static bool miniapp_buzzer_stream_should_abort(uint32_t generation, uint32_t melody_gen)
{
    return !s_active || generation != s_buzzer_generation || melody_gen != s_melody_gen ||
           voice_player_tts_is_active() || alarm_ring_is_active();
}

/* 收尾。completed = true 表示音符都写完了, 发 EOS 让播放器把缓冲里的尾巴放完。
 *
 * 这里**绝对不能** reset: 实机实测一首 6 秒的曲子, 全部 PCM 会在 ~0.2s 内就写进
 * 播放器缓冲区, 那时候缓冲区里还压着 5.9 秒的音频 —— reset 会把它们全部砍掉,
 * 一首 6 秒的歌只剩 1 秒。
 *
 * 等多久也不能看播放器状态: finish_stream 一发出, 状态立刻变"结束", 而缓冲区还在
 * 放 —— 拿状态当判据会让写线程在声音没停的时候就撒手, 之后"按停止"就没人搭理了。
 * 所以按**时长**守着(期望时长 = 写进去的采样数), 期间随时可以被中止打断:
 * 停止、换曲、退出、来了一首新的、TTS/闹钟插进来。 */
/* 整曲开始处理的时刻，以及"播放位置第一次大于 0"的相对时刻。前者用来量端到端
 * 延迟，后者用来量"从按下播放到声音真的响起来"那段固定延迟 —— Lua 侧的进度条和
 * 计时是从按下播放起算的，UI 就是领先声音这么多。 */
static TickType_t s_melody_start_tick;
static volatile uint32_t s_melody_audio_start_ms;

static void miniapp_buzzer_stream_end(bool completed, uint32_t expected_ms,
                                      uint32_t generation, uint32_t melody_gen)
{
    if (!completed || app_player_finish_stream(miniapp_player) != APP_PLAYER_OK) {
        (void)app_player_reset(miniapp_player);
        return;
    }
    TickType_t started = xTaskGetTickCount();
    TickType_t cap = pdMS_TO_TICKS(expected_ms + 3000u);
    uint32_t played_max = 0;       /* 播放器自己报的播放位置（最大值） */
    uint32_t ended_at = 0;         /* 播放位置走满的时刻 */
    while (xTaskGetTickCount() - started < cap) {
        if (miniapp_buzzer_stream_should_abort(generation, melody_gen)) {
            miniapp_buzzer_silence();
            LISA_LOGI(TAG, "melody: tail cut short by abort");
            if (miniapp_buzzer_player_active()) {
                LISA_LOGW(TAG, "melody: player still active after abort");
            }
            return;
        }
        /* 状态不再是"在播" = 声音真的放完了(比"位置走满"可靠: 位置比实际输出
         * 大约超前一个解码缓冲区, 拿它当判据会掐掉最后一段)。 */
        if (!miniapp_buzzer_player_active()) {
            ended_at = xTaskGetTickCount() - s_melody_start_tick;
            break;
        }
        uint32_t pos = 0;
        if (app_player_get_position(miniapp_player, &pos) == APP_PLAYER_OK && pos > played_max) {
            played_max = pos;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    /* 这三条一起回答了"音乐到底放完没有、UI 差多少":
     *   pos/expected —— 有没有丢音频(远小于 expected 就是丢了, 而不是曲谱有休止)
     *   start +N ms  —— 从按下播放到**声音真的响起来**的固定延迟。Lua 侧的进度条
     *                   和计时是从按下播放起算的, 所以 UI 就领先声音这么多。
     *   end +M ms    —— 声音放完的时刻; 正常情况下 M ≈ N + expected。 */
    LISA_LOGI(TAG,
              "melody: played %u/%u ms, audio start +%u ms, end +%u ms (ui leads audio by %u ms)",
              (unsigned)played_max, (unsigned)expected_ms,
              (unsigned)s_melody_audio_start_ms, (unsigned)pdTICKS_TO_MS(ended_at),
              (unsigned)s_melody_audio_start_ms);
    /* 正常情况下声音这时候已经放完了; 真要是还响着也补一刀, 免得留个没人管的
     * 尾巴。 */
    if (miniapp_buzzer_player_active()) {
        LISA_LOGW(TAG, "melody: still active after %u ms, silencing", (unsigned)expected_ms);
        miniapp_buzzer_silence();
    }
}

/* 流式播放一段音符表。返回 0 = 播完, 1 = 被中止, 2 = 流式不可用(调用方回退)。
 *
 * 写入要认返回值: app_player_write_stream 返回的是**实际写入的字节数**, 可能小于
 * 请求量、也可能为 0(缓冲区满)。不认它就会静默丢音频。这里按实际写入量推进,
 * 写不进去就等一会儿再试, 相位只按"真正送出去的采样数"推进, 重试时重新生成尾部。 */
static int miniapp_buzzer_stream_notes(const miniapp_note_t *notes, unsigned count,
                                       uint32_t generation, uint32_t melody_gen,
                                       uint8_t *block)
{
    if (app_player_play_stream(miniapp_player, 16000u, 1u, 16u) != APP_PLAYER_OK) {
        LISA_LOGW(TAG, "melody: play_stream failed, falling back to one WAV per note");
        return 2;
    }

    const uint32_t block_samples = MINIAPP_BUZZER_SEQ_BLOCK_MS * 16u;
    /* 写入的总时长兜底: 万一音频输出没起来(例如别的播放器占着 PA), 缓冲区就永远
     * 排不空, write_stream 会把任务卡死在这儿。给"整曲时长 + 5 秒"的额度, 超了
     * 就认输, 把控制权还给任务。 */
    TickType_t write_started = xTaskGetTickCount();
    uint32_t total_sane = 0;
    for (unsigned i = 0; i < count; ++i) total_sane += notes[i].duration_ms;
    uint32_t write_deadline_ticks = (uint32_t)pdMS_TO_TICKS(total_sane + 5000u);
    uint32_t phase = 0;
    bool first_write = true;
    unsigned stalls = 0;
    uint32_t written_samples = 0;   /* 真正送进播放器的采样数(用于核对有没有丢) */
    uint32_t wanted_samples = 0;

    for (unsigned i = 0; i < count; ++i) {
        uint32_t total = (uint32_t)notes[i].duration_ms * 16u;   /* 16 采样/ms */
        uint16_t freq = notes[i].frequency_hz;
        wanted_samples += total;
        uint32_t sent = 0;
        while (sent < total) {
            if (miniapp_buzzer_stream_should_abort(generation, melody_gen)) {
                LISA_LOGI(TAG, "melody: aborted at note %u/%u", i, count);
                miniapp_buzzer_stream_end(false, 0, generation, melody_gen);
                return 1;
            }
            uint32_t batch = total - sent;
            if (batch > block_samples) batch = block_samples;
            /* 与单音路径逐字节一致的方波: 相位跨块、跨音符连续, 所以同一个音被切成
             * 几块也不会在接缝处听到跳变。freq=0 是休止, 写静音(不是 0Hz 的方波)。 */
            uint32_t ph = phase;
            for (uint32_t s = 0; s < batch; ++s) {
                int16_t sample = 0;
                if (freq) {
                    ph = (ph + freq) % 16000u;
                    sample = ph < 8000u ? 12000 : -12000;
                }
                block[s * 2u] = (uint8_t)sample;
                block[s * 2u + 1u] = (uint8_t)((uint16_t)sample >> 8);
            }
            int wrote = app_player_write_stream(miniapp_player, block, batch * 2u,
                                                MINIAPP_BUZZER_SEQ_BLOCK_MS);
            if (wrote < 0) {
                LISA_LOGW(TAG, "melody: write_stream failed (%d) at note %u/%u", wrote, i, count);
                miniapp_buzzer_stream_end(false, 0, generation, melody_gen);
                /* 一块都没写进去, 说明这次流压根没起来(比如播放器不在
                 * FOREGROUND 时 play_stream 会假成功) —— 交给回退路径。 */
                return first_write ? 2 : 1;
            }
            first_write = false;
            uint32_t accepted = (uint32_t)wrote / 2u;      /* 字节 -> 采样 */
            if ((uint32_t)(xTaskGetTickCount() - write_started) > write_deadline_ticks) {
                LISA_LOGW(TAG, "melody: write stalled past %u ms, giving up at note %u/%u",
                          (unsigned)(total_sane + 5000u), i, count);
                miniapp_buzzer_stream_end(false, 0, generation, melody_gen);
                return 1;
            }
            if (accepted == 0) {
                /* 缓冲区满了。等它被消费掉一点再重试同一块。 */
                if (++stalls == 1) {
                    LISA_LOGI(TAG, "melody: player buffer full at note %u/%u", i, count);
                }
                vTaskDelay(pdMS_TO_TICKS(MINIAPP_BUZZER_SEQ_BLOCK_MS));
                continue;
            }
            if (freq) phase = (phase + freq * accepted) % 16000u;
            sent += accepted;
            written_samples += accepted;
            /* 播放位置第一次大于 0 = 声音真的开始响了 */
            if (!s_melody_audio_start_ms) {
                uint32_t pos = 0;
                if (app_player_get_position(miniapp_player, &pos) == APP_PLAYER_OK && pos > 0) {
                    s_melody_audio_start_ms =
                        (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - s_melody_start_tick);
                }
            }
        }
    }
    LISA_LOGI(TAG, "melody: done, %u notes, %u/%u samples(%u stalls)",
              count, (unsigned)written_samples, (unsigned)wanted_samples, stalls);
    /* 期望时长 = 写进去的采样数 / 16; 播放器把缓冲区放完大约就是这个时间 */
    miniapp_buzzer_stream_end(true, written_samples / 16u, generation, melody_gen);
    return 0;
}

/* 回退: 把同一段音符表按单音逐个放完。节奏会退化回"每音一个 WAV"(每音约
 * 50ms 空隙), 但至少不是一片静音。只在流式起不来时走。 */
static void miniapp_buzzer_play_notes_as_wavs(const miniapp_note_t *notes, unsigned count,
                                              uint8_t *wav, uint32_t generation)
{
    for (unsigned i = 0; i < count; ++i) {
        if (miniapp_buzzer_stream_should_abort(generation, s_melody_gen)) return;
        if (notes[i].frequency_hz == 0) {
            /* 休止: 单音路径没法"播静音", 用等长的延时代替 */
            TickType_t started = xTaskGetTickCount();
            while (xTaskGetTickCount() - started < pdMS_TO_TICKS(notes[i].duration_ms)) {
                if (miniapp_buzzer_stream_should_abort(generation, s_melody_gen)) return;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        } else {
            miniapp_buzzer_play_wav_note(wav, notes[i].frequency_hz, notes[i].duration_ms, generation);
        }
    }
}

/* 整曲播放的主流程。返回后 s_melody_playing 一定被清掉(除非期间又来了一首,
 * 那时会就地开始新的那首)。 */
static void miniapp_buzzer_run_melody(uint8_t *wav, uint8_t *block, char *text,
                                      miniapp_note_t *notes)
{
    for (unsigned round = 0; round < 4; ++round) {
        uint32_t generation = s_buzzer_generation;
        uint32_t melody_gen = 0;
        int count = 0;

        /* 文本只有这里和 Lua 侧会碰, 全在临界区里搬 —— 一次 memcpy(约 3KB),
         * 解析放在临界区外面做(有乘加循环, 不该占着中断)。 */
        taskENTER_CRITICAL();
        melody_gen = s_melody_gen;
        memcpy(text, s_melody_text, sizeof(s_melody_text));
        taskEXIT_CRITICAL();

        text[MINIAPP_BUZZER_SEQ_MAX_BYTES - 1] = '\0';
        count = miniapp_buzzer_parse_notes(text, notes, MINIAPP_BUZZER_SEQ_MAX_NOTES);

        /* 文本在解析期间又换了 → 重新来一遍, 以最新的为准。 */
        if (melody_gen != s_melody_gen) continue;

        if (count <= 0) {          /* 空串 = 停止 */
            /* "停止"必须**自己**动手: 全部 PCM 在 ~0.2s 内就写完了, 之后播放器
             * 状态立刻变"结束"而缓冲区里还压着整首歌 —— 写线程早就退出, 没人
             * 再看管它。所以这里只是"顺手记一笔"是不行的, 实机表现就是
             * "播放中按停止没反应"。 */
            miniapp_buzzer_silence();
            LISA_LOGI(TAG, "melody: stopped by request");
            s_melody_playing = false;
            return;
        }
        if (!s_active || generation != s_buzzer_generation ||
            voice_player_tts_is_active() || alarm_ring_is_active()) {
            miniapp_buzzer_silence();
            s_melody_playing = false;
            return;
        }

        /* 上一个音(单音或上一首整曲)可能还占着播放器 */
        miniapp_buzzer_silence();

        s_melody_start_tick = xTaskGetTickCount();
        s_melody_audio_start_ms = 0;
        LISA_LOGI(TAG, "melody: start, %d notes, gen=%u", count, (unsigned)melody_gen);
        int rc = miniapp_buzzer_stream_notes(notes, (unsigned)count, generation, melody_gen, block);
        if (rc == 2) {
            miniapp_buzzer_play_notes_as_wavs(notes, (unsigned)count, wav, generation);
        }

        /* 播完/被中止之后再确认一次: 期间是否又来了一首? */
        if (melody_gen == s_melody_gen) {
            s_melody_playing = false;
            return;
        }
    }
    LISA_LOGW(TAG, "melody: gave up after repeated restarts");
    s_melody_playing = false;
}

static void miniapp_buzzer_task(void *argument)
{
    (void)argument;
    miniapp_buzzer_command_t command;
    /* The decoder retains the memory URL. Keep one bounded buffer alive and
     * synchronously stop its dedicated player before overwriting any bytes. */
    uint8_t *wav = NULL;
    /* 整曲路径的两块内存: 送进播放器的 PCM 分块, 以及从信箱里搬出来的文本副本
     * (不能在临界区里解析, 也不该把 3KB 放在这 8KB 的任务栈上)。 */
    uint8_t *block = NULL;
    char *text = NULL;
    miniapp_note_t *notes = NULL;
    for (;;) {
        if (xQueueReceive(s_buzzer_queue, &command, portMAX_DELAY) != pdTRUE) continue;
        if (!s_buzzer_queue) continue;
        if (voice_player_tts_is_active() || alarm_ring_is_active()) {
            if (command.kind == MINIAPP_BUZZER_KIND_SEQ) s_melody_playing = false;
            continue;
        }
        if (!s_active || command.generation != s_buzzer_generation || !miniapp_player) {
            if (command.kind == MINIAPP_BUZZER_KIND_SEQ) s_melody_playing = false;
            continue;
        }
        if (command.kind == MINIAPP_BUZZER_KIND_SEQ) {
            if (!block) block = lisa_mem_alloc(MINIAPP_BUZZER_SEQ_BLOCK_MS * 32u);
            if (!text) text = lisa_mem_alloc(MINIAPP_BUZZER_SEQ_MAX_BYTES);
            if (!notes) notes = lisa_mem_alloc(MINIAPP_BUZZER_SEQ_MAX_NOTES * sizeof(*notes));
            /* wav 只在"流式起不来"的回退路径上用得上, 但必须在进 run_melody
             * 之前就备好 —— 回退是在里面发起的, 那里没有地方再分配。 */
            if (!wav) wav = lisa_mem_alloc(44u + MINIAPP_BUZZER_MAX_MS * 32u);
            if (!block || !text || !notes || !wav) {
                LISA_LOGW(TAG, "melody: not enough memory, dropping");
                s_melody_playing = false;
                continue;
            }
            miniapp_buzzer_run_melody(wav, block, text, notes);
            continue;
        }
        if (miniapp_buzzer_player_active() && app_player_stop(miniapp_player) != APP_PLAYER_OK) {
            LISA_LOGW(TAG, "failed to stop previous buzzer");
            continue;
        }
        if (!wav) wav = lisa_mem_alloc(44u + MINIAPP_BUZZER_MAX_MS * 32u);
        if (!wav) {
            LISA_LOGW(TAG, "failed to allocate buzzer buffer");
            continue;
        }
        miniapp_buzzer_play_wav_note(wav, command.frequency_hz, command.duration_ms,
                                     command.generation);
    }
}
#endif

static void miniapp_storage_fault(miniapp_vm_t *vm)
{
    vm->faulted = true;
    vm->storage_pending = 0;
    cJSON_Delete(vm->storage_json);
    vm->storage_json = NULL;
    if (vm->storage_read) {
        const char *error = miniapp_storage_clear(vm->id);
        if (error) LISA_LOGW(TAG, "failed to clear app data: %s", error);
    }
}

static int miniapp_lua_clock_now(lua_State *L)
{
    struct timeval tv;
    if (!ls_sys_time_is_valid() || ls_sys_get_time(&tv) != 0) lua_pushnil(L);
    else lua_pushinteger(L, tv.tv_sec);
    return 1;
}

static int miniapp_lua_clock_localtime(lua_State *L)
{
    struct tm calendar;
    if (ls_sys_get_localtime(&calendar) != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 8);
#define CLOCK_FIELD(name, value) do { lua_pushinteger(L, value); lua_setfield(L, -2, name); } while (0)
    CLOCK_FIELD("year", calendar.tm_year + 1900);
    CLOCK_FIELD("month", calendar.tm_mon + 1);
    CLOCK_FIELD("day", calendar.tm_mday);
    CLOCK_FIELD("hour", calendar.tm_hour);
    CLOCK_FIELD("min", calendar.tm_min);
    CLOCK_FIELD("sec", calendar.tm_sec);
    CLOCK_FIELD("wday", calendar.tm_wday);
    CLOCK_FIELD("utc_offset", ls_sys_timezone_offset_seconds());
#undef CLOCK_FIELD
    return 1;
}

static int miniapp_storage_result(lua_State *L, const char *error)
{
    lua_pushboolean(L, error == NULL);
    if (error) { lua_pushstring(L, error); return 2; }
    return 1;
}

static bool miniapp_storage_object_valid(cJSON *object)
{
    if (!cJSON_IsObject(object)) return false;
    unsigned count = 0;
    for (cJSON *item = object->child; item; item = item->next) {
        if (++count > 32 || !item->string || !*item->string || strlen(item->string) > 32) return false;
        if (cJSON_IsNumber(item)) { if (!isfinite(item->valuedouble)) return false; }
        else if (cJSON_IsString(item)) { if (strlen(item->valuestring) > 256) return false; }
        else if (!cJSON_IsBool(item)) return false;
    }
    return true;
}

static int miniapp_lua_storage_load(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->faulted || vm->storage_pending == 2 || !ls_sys_time_is_valid()) { lua_pushnil(L); return 1; }
    size_t size = vm->storage_size;
    if (vm->storage_pending != 1 && !miniapp_storage_load(vm->id, vm->storage_data, &size)) {
        lua_pushnil(L);
        return 1;
    }
    vm->storage_json = cJSON_ParseWithLength(vm->storage_data, size + 1);
    if (!miniapp_storage_object_valid(vm->storage_json)) {
        cJSON_Delete(vm->storage_json);
        vm->storage_json = NULL;
        (void)miniapp_storage_clear(vm->id);
        lua_pushnil(L);
        return 1;
    }
    vm->storage_read = true;
    lua_newtable(L);
    for (cJSON *item = vm->storage_json->child; item; item = item->next) {
        if (cJSON_IsNumber(item)) lua_pushnumber(L, item->valuedouble);
        else if (cJSON_IsBool(item)) lua_pushboolean(L, cJSON_IsTrue(item));
        else lua_pushstring(L, item->valuestring);
        lua_setfield(L, -2, item->string);
    }
    cJSON_Delete(vm->storage_json);
    vm->storage_json = NULL;
    return 1;
}

static int miniapp_lua_storage_save(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->faulted || vm->retiring) return miniapp_storage_result(L, "unavailable");
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_Integer ttl = luaL_optinteger(L, 2, MINIAPP_STORAGE_DEFAULT_TTL);
    if (ttl < 1 || ttl > MINIAPP_STORAGE_MAX_TTL) return miniapp_storage_result(L, "invalid_ttl");
    if (!ls_sys_time_is_valid()) return miniapp_storage_result(L, "clock_unavailable");
    vm->storage_json = cJSON_CreateObject();
    if (!vm->storage_json) return miniapp_storage_result(L, "no_memory");
    const char *error = NULL;
    unsigned count = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        size_t key_len = 0, value_len = 0;
        const char *key = lua_type(L, -2) == LUA_TSTRING ? lua_tolstring(L, -2, &key_len) : NULL;
        cJSON *item = NULL;
        if (++count > 32 || !key || !key_len || key_len > 32 || memchr(key, 0, key_len)) {
            error = "invalid_data";
        } else if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
            item = cJSON_CreateNumber(lua_tonumber(L, -1));
        } else if (lua_type(L, -1) == LUA_TBOOLEAN) {
            item = cJSON_CreateBool(lua_toboolean(L, -1));
        } else if (lua_type(L, -1) == LUA_TSTRING) {
            const char *value = lua_tolstring(L, -1, &value_len);
            if (value_len > 256 || memchr(value, 0, value_len)) error = "invalid_data";
            else item = cJSON_CreateString(value);
        } else error = "invalid_data";
        if (!error && (!item || !cJSON_AddItemToObject(vm->storage_json, key, item))) {
            cJSON_Delete(item);
            error = "no_memory";
        }
        lua_pop(L, 1);
        if (error) { lua_pop(L, 1); break; }
    }
    /* Never replace pending data on serialization failure. */
    char *encoded = !error ? cJSON_PrintUnformatted(vm->storage_json) : NULL;
    if (!error && !encoded) error = "no_memory";
    if (!error && strlen(encoded) > MINIAPP_STORAGE_MAX_BYTES) error = "too_large";
    if (!error) {
        if (vm->validating) {
            vm->storage_size = strlen(encoded);
            memcpy(vm->storage_data, encoded, vm->storage_size + 1);
            vm->storage_ttl = ttl;
            vm->storage_pending = 1;
        } else error = miniapp_storage_save(vm->id, encoded, strlen(encoded), ttl);
    }
    cJSON_free(encoded);
    cJSON_Delete(vm->storage_json);
    vm->storage_json = NULL;
    return miniapp_storage_result(L, error);
}

static int miniapp_lua_storage_clear(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->faulted || vm->retiring) return miniapp_storage_result(L, "unavailable");
    if (vm->validating) { vm->storage_pending = 2; return miniapp_storage_result(L, NULL); }
    return miniapp_storage_result(L, miniapp_storage_clear(vm->id));
}

static bool miniapp_http_is_cancelled(miniapp_http_request_t *request)
{
    bool cancelled;
    bool queued;
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    cancelled = request->cancelled;
    xSemaphoreGive(s_http_lock);
    return cancelled;
}

static void miniapp_http_remove(miniapp_http_request_t *request)
{
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    for (size_t i = 0; i < MINIAPP_HTTP_MAX_REQUESTS; ++i) {
        if (s_http_requests[i] == request) s_http_requests[i] = NULL;
    }
    xSemaphoreGive(s_http_lock);
}

static void miniapp_http_free(miniapp_http_request_t *request,
                              miniapp_http_response_t *response)
{
    if (response) {
        lisa_mem_free(response->body);
    }
    if (request) {
        lisa_mem_free(request->body);
        lisa_mem_free(request);
    }
}

static void miniapp_http_error(miniapp_http_response_t *response, const char *code)
{
    response->error = -1;
    snprintf(response->error_code, sizeof(response->error_code), "%s", code);
    snprintf(response->error_message, sizeof(response->error_message), "%s", code);
}

static bool miniapp_http_header(HTTP_SESSION_HANDLE handle, char *name,
                                char *value, size_t capacity)
{
    char line[256];
    UINT32 length = sizeof(line) - 1;
    HTTPClientFindFirstHeader(handle, name, line, &length);
    int rc = HTTPClientGetNextHeader(handle, line, &length);
    HTTPClientFindCloseHeader(handle);
    if (rc != HTTP_CLIENT_SUCCESS) return false;
    char *start = strchr(line, ':');
    if (!start) return false;
    do { ++start; } while (*start == ' ' || *start == '\t');
    size_t size = strcspn(start, "\r\n");
    while (size && (start[size - 1] == ' ' || start[size - 1] == '\t')) --size;
    snprintf(value, capacity, "%.*s", (int)size, start);
    return true;
}

static UINT32 miniapp_http_timeout(miniapp_http_request_t *request, TickType_t start)
{
    uint32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
    return elapsed < request->timeout_ms ? (request->timeout_ms - elapsed + 999u) / 1000u : 0;
}

static void miniapp_http_execute(miniapp_http_response_t *response)
{
    miniapp_http_request_t *request = response->request;
    TickType_t started = xTaskGetTickCount();
    HTTP_SESSION_HANDLE handle = HTTPClientOpenRequest(0);
    if (!handle) { miniapp_http_error(response, "unavailable"); return; }
    int rc = HTTPClientSetVerb(handle, request->method == LISA_HTTP_POST ? VerbPost : VerbGet);
    if (rc != HTTP_CLIENT_SUCCESS) goto done;
    /* Add headers individually: the SDK's extension callback uses '&' as
     * a delimiter, which would corrupt otherwise valid header values. */
    for (char *line = request->headers; *line;) {
        char *colon = strchr(line, ':');
        char *end = strstr(line, "\r\n");
        if (!colon || !end || colon > end) { miniapp_http_error(response, "invalid_argument"); goto done; }
        *colon = 0;
        *end = 0;
        rc = HTTPClientAddRequestHeaders(handle, line, colon + 2, FALSE);
        *colon = ':';
        *end = '\r';
        if (rc != HTTP_CLIENT_SUCCESS) goto done;
        line = end + 2;
    }
    rc = HTTPClientSendRequest(handle, request->url, request->body,
                              request->body_len, request->method == LISA_HTTP_POST,
                              miniapp_http_timeout(request, started), 0,
                              NULL, NULL);
    if (rc != HTTP_CLIENT_SUCCESS || miniapp_http_is_cancelled(request)) goto done;
    UINT32 timeout = miniapp_http_timeout(request, started);
    if (!timeout) { miniapp_http_error(response, "timeout"); goto done; }
    /* The convenience HTTPC_request follows redirects. Use the same SDK
     * session directly so 3xx/4xx/5xx are returned to the Lua application. */
    rc = HTTPClientRecvResponse(handle, timeout);
    if (rc != HTTP_CLIENT_SUCCESS) goto done;
    HTTP_CLIENT info;
    rc = HTTPClientGetInfo(handle, &info);
    if (rc != HTTP_CLIENT_SUCCESS) goto done;
    response->status = info.HTTPStatusCode;
    miniapp_http_header(handle, "Content-Type", response->content_type,
                        sizeof(response->content_type));
    if (info.TotalResponseBodyLength > request->max_response_bytes) {
        miniapp_http_error(response, "response_too_large"); goto done;
    }
    char length_header[32];
    bool empty = miniapp_http_header(handle, "Content-Length", length_header,
                                     sizeof(length_header)) && !strcmp(length_header, "0");
    if (empty || response->status == 204 || response->status == 304) goto done;
    for (;;) {
        char buffer[513];
        UINT32 received = 0;
        timeout = miniapp_http_timeout(request, started);
        if (!timeout) { miniapp_http_error(response, "timeout"); break; }
        if (miniapp_http_is_cancelled(request)) break;
        rc = HTTPClientReadData(handle, buffer, sizeof(buffer) - 1, timeout, &received);
        if (received > request->max_response_bytes - response->body_len) {
            miniapp_http_error(response, "response_too_large"); break;
        }
        memcpy(response->body + response->body_len, buffer, received);
        response->body_len += received;
        response->body[response->body_len] = 0;
        if (rc == HTTP_CLIENT_EOS || rc != HTTP_CLIENT_SUCCESS) break;
    }
done:
    HTTPClientCloseRequest(&handle);
    if (!response->error) {
        if (!miniapp_http_timeout(request, started) || rc == HTTP_CLIENT_ERROR_SOCKET_TIME_OUT)
            miniapp_http_error(response, "timeout");
        else if (rc != HTTP_CLIENT_SUCCESS && rc != HTTP_CLIENT_EOS)
            miniapp_http_error(response, "network_error");
    }
}

static void miniapp_http_task(void *argument)
{
    (void)argument;
    for (;;) {
        miniapp_http_request_t *request = NULL;
        if (xQueueReceive(s_http_queue, &request, portMAX_DELAY) != pdTRUE || !request) continue;
        if (miniapp_http_is_cancelled(request)) {
            miniapp_http_remove(request);
            miniapp_http_free(request, NULL);
            continue;
        }
        /* The result record was allocated with the accepted request. Even
         * response-body allocation failure has a durable error result. */
        miniapp_http_response_t *response = &request->response;
        response->request = request;
        response->body = lisa_mem_calloc(1, request->max_response_bytes + 1u);
        if (!response->body) miniapp_http_error(response, "unavailable");
        else miniapp_http_execute(response);
        LISA_LOGI(TAG, "HTTP result id=%u status=%u bytes=%u error=%s",
                  (unsigned)request->id, response->status,
                  (unsigned)response->body_len, response->error_code);
        xSemaphoreTake(s_http_lock, portMAX_DELAY);
        request->complete = true;
        xSemaphoreGive(s_http_lock);
    }
}

/* Completed results remain owned by their request while the app is frozen. */
static miniapp_http_response_t *miniapp_http_take_result(miniapp_vm_t *vm, bool frozen)
{
    miniapp_http_response_t *result = NULL;
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    for (unsigned i = 0; i < MINIAPP_HTTP_MAX_REQUESTS; ++i) {
        miniapp_http_request_t *r = s_http_requests[i];
        if (r && r->complete && (!frozen || r->cancelled || !vm ||
                                r->generation != vm->http_generation)) {
            s_http_requests[i] = NULL;
            result = &r->response;
            break;
        }
    }
    xSemaphoreGive(s_http_lock);
    return result;
}

/* Candidate startup may create requests but cannot send them before commit.
 * Retiring one VM only cancels requests owned by that instance. */
static void miniapp_http_discard(miniapp_vm_t *vm)
{
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    for (size_t i = 0; i < MINIAPP_HTTP_MAX_REQUESTS; ++i) {
        miniapp_http_request_t *r = s_http_requests[i];
        if (!r || r->generation != vm->http_generation) continue;
        r->cancelled = true;
        if (!r->queued || r->complete) {
            s_http_requests[i] = NULL;
            miniapp_http_free(r, &r->response);
        }
    }
    xSemaphoreGive(s_http_lock);
}

static void miniapp_http_activate(miniapp_vm_t *vm)
{
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    for (size_t i = 0; i < MINIAPP_HTTP_MAX_REQUESTS; ++i) {
        miniapp_http_request_t *r = s_http_requests[i];
        if (!r || r->generation != vm->http_generation || r->queued) continue;
        /* Registry and worker queue have the same capacity. */
        r->queued = xQueueSend(s_http_queue, &r, 0) == pdTRUE;
    }
    xSemaphoreGive(s_http_lock);
}

static int miniapp_http_options(lua_State *L, int index, const char *url,
                                miniapp_http_request_t *request)
{
    size_t url_len = strlen(url);
    if (!url_len || url_len > MINIAPP_HTTP_MAX_URL_BYTES ||
        (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) {
        return luaL_error(L, "invalid http URL");
    }
    snprintf(request->url, sizeof(request->url), "%s", url);
    request->method = LISA_HTTP_GET;
    request->timeout_ms = MINIAPP_HTTP_DEFAULT_TIMEOUT_MS;
    request->max_response_bytes = MINIAPP_HTTP_DEFAULT_RESPONSE_BYTES;
    /* Omitted headers use the SDK defaults without a provider callback. */
    request->headers[0] = 0;
    if (index != 0 && !lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua_getfield(L, index, "method");
        if (!lua_isnil(L, -1)) {
            const char *method = luaL_checkstring(L, -1);
            if (!strcmp(method, "POST")) request->method = LISA_HTTP_POST;
            else if (strcmp(method, "GET")) { lua_pop(L, 1); return luaL_error(L, "method must be GET or POST"); }
        }
        lua_pop(L, 1);
        lua_getfield(L, index, "timeout_ms");
        if (!lua_isnil(L, -1)) {
            lua_Integer value = luaL_checkinteger(L, -1);
            if (value < 1 || value > MINIAPP_HTTP_MAX_TIMEOUT_MS) { lua_pop(L, 1); return luaL_error(L, "invalid timeout_ms"); }
            request->timeout_ms = (uint32_t)value;
        }
        lua_pop(L, 1);
        lua_getfield(L, index, "max_response_bytes");
        if (!lua_isnil(L, -1)) {
            lua_Integer value = luaL_checkinteger(L, -1);
            if (value < 1 || value > MINIAPP_HTTP_MAX_RESPONSE_BYTES) { lua_pop(L, 1); return luaL_error(L, "invalid max_response_bytes"); }
            request->max_response_bytes = (size_t)value;
        }
        lua_pop(L, 1);
        lua_getfield(L, index, "body");
        if (!lua_isnil(L, -1)) {
            size_t len = 0;
            const char *body = luaL_checklstring(L, -1, &len);
            if (request->method != LISA_HTTP_POST || len > MINIAPP_HTTP_MAX_BODY_BYTES) { lua_pop(L, 1); return luaL_error(L, "body is only valid for POST and must be <= 8192 bytes"); }
            request->body = lisa_mem_alloc(len + 1u);
            if (!request->body) { lua_pop(L, 1); return luaL_error(L, "no memory"); }
            memcpy(request->body, body, len); request->body[len] = '\0'; request->body_len = len;
        }
        lua_pop(L, 1);
        lua_getfield(L, index, "headers");
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TTABLE);
            size_t used = 0, count = 0;
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                size_t key_len = 0, value_len = 0;
                if (lua_type(L, -2) != LUA_TSTRING) return luaL_error(L, "header names must be strings");
                const char *key = luaL_checklstring(L, -2, &key_len);
                const char *value = luaL_checklstring(L, -1, &value_len);
                if (memchr(key, 0, key_len) || strpbrk(key, "\r\n:") ||
                    memchr(value, 0, value_len) || strpbrk(value, "\r\n"))
                    return luaL_error(L, "invalid header characters");
                size_t add = key_len + value_len + 4u;
                if (++count > MINIAPP_HTTP_MAX_HEADERS || !key_len || used + add > MINIAPP_HTTP_MAX_HEADER_BYTES) {
                    lua_pop(L, 2); return luaL_error(L, "invalid headers");
                }
                used += (size_t)snprintf(request->headers + used, sizeof(request->headers) - used, "%.*s: %.*s\r\n", (int)key_len, key, (int)value_len, value);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    if (request->method == LISA_HTTP_POST && !request->body) {
        request->body = lisa_mem_calloc(1, 1);
    }
    return 0;
}

static int miniapp_http_options_protected(lua_State *L)
{
    miniapp_http_request_t *request = lua_touserdata(L, 1);
    return miniapp_http_options(L, lua_isnil(L, 2) ? 0 : 2,
                                lua_tostring(L, 3), request);
}

static int miniapp_lua_http_request_common(lua_State *L, int url_index, int options_index)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    url_index = lua_absindex(L, url_index);
    size_t url_len;
    const char *url = luaL_checklstring(L, url_index, &url_len);
    if (memchr(url, 0, url_len) || strpbrk(url, "\r\n")) return luaL_error(L, "invalid http URL");
    if (!lua_checkstack(L, 4)) return luaL_error(L, "not enough Lua stack");
    if (vm->faulted || vm->retiring || !s_http_queue) return luaL_error(L, "http unavailable");
    miniapp_http_request_t *request = lisa_mem_calloc(1, sizeof(*request));
    if (!request) return luaL_error(L, "no memory");
    lua_pushcfunction(L, miniapp_http_options_protected);
    lua_pushlightuserdata(L, request);
    if (options_index) lua_pushvalue(L, options_index); else lua_pushnil(L);
    lua_pushvalue(L, url_index);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        miniapp_http_free(request, NULL);
        return lua_error(L);
    }
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    size_t slot = MINIAPP_HTTP_MAX_REQUESTS;
    for (size_t i = 0; i < MINIAPP_HTTP_MAX_REQUESTS; ++i) if (!s_http_requests[i]) { slot = i; break; }
    if (slot == MINIAPP_HTTP_MAX_REQUESTS) { xSemaphoreGive(s_http_lock); miniapp_http_free(request, NULL); return luaL_error(L, "too many http requests"); }
    request->id = s_http_next_id++;
    if (!request->id) request->id = s_http_next_id++;
    request->generation = vm->http_generation;
    s_http_requests[slot] = request;
    xSemaphoreGive(s_http_lock);
    uint32_t id = request->id;
    if (!vm->validating) {
        request->queued = true;
        if (xQueueSend(s_http_queue, &request, 0) != pdTRUE) {
            miniapp_http_remove(request); miniapp_http_free(request, NULL);
            return luaL_error(L, "http queue is busy");
        }
    }
    lua_pushinteger(L, id);
    return 1;
}

static int miniapp_lua_http_request(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "url");
    int result = miniapp_lua_http_request_common(L, -1, 1);
    lua_remove(L, -2); /* remove URL, preserve the returned request ID */
    return result;
}

static int miniapp_lua_http_get(lua_State *L)
{
    return miniapp_lua_http_request_common(L, 1, lua_isnoneornil(L, 2) ? 0 : 2);
}

static int miniapp_lua_http_cancel(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool cancelled = false;
    xSemaphoreTake(s_http_lock, portMAX_DELAY);
    for (size_t i = 0; i < MINIAPP_HTTP_MAX_REQUESTS; ++i) {
        miniapp_http_request_t *r = s_http_requests[i];
        if (r && r->id == id && r->generation == miniapp_vm_get(L)->http_generation && !r->cancelled) {
            r->cancelled = true;
            cancelled = true;
            if (!r->queued) { s_http_requests[i] = NULL; miniapp_http_free(r, NULL); }
            break;
        }
    }
    xSemaphoreGive(s_http_lock);
    lua_pushboolean(L, cancelled);
    return 1;
}

#define MINIAPP_JSON_MAX_DEPTH 32u
#define MINIAPP_JSON_MAX_BYTES MINIAPP_LUA_HEAP_LIMIT

static char null_key, array_key;
typedef struct {
    cJSON *root;
    char *printed;
    const char *error;
    const void *ancestors[MINIAPP_JSON_MAX_DEPTH + 1];
    size_t bytes;
} json_call_t;

static void fill_json(lua_State *L, int index, cJSON *out, json_call_t *c, unsigned depth)
{
    index = lua_absindex(L, index);
    if (!lua_checkstack(L, 6)) { luaL_error(L, "not enough Lua stack"); return; }
    if (c->bytes > MINIAPP_JSON_MAX_BYTES - 8) { c->error = "too_large"; return; }
    c->bytes += 8;
    if (depth > MINIAPP_JSON_MAX_DEPTH) { c->error = "too_deep"; return; }
    if (lua_type(L, index) == LUA_TLIGHTUSERDATA && lua_touserdata(L, index) == &null_key) {
        out->type = cJSON_NULL;
    } else switch (lua_type(L, index)) {
    case LUA_TBOOLEAN:
        out->type = lua_toboolean(L, index) ? cJSON_True : cJSON_False;
        break;
    case LUA_TNUMBER:
        if (!isfinite(lua_tonumber(L, index))) { c->error = "invalid_value"; break; }
        out->type = cJSON_Number;
        cJSON_SetNumberValue(out, lua_tonumber(L, index));
        break;
    case LUA_TSTRING: {
        size_t len;
        const char *s = lua_tolstring(L, index, &len);
        if (memchr(s, 0, len)) { c->error = "invalid_value"; break; }
        if (len > MINIAPP_JSON_MAX_BYTES - c->bytes) { c->error = "too_large"; break; }
        c->bytes += len;
        out->valuestring = cJSON_malloc(len + 1);
        if (!out->valuestring) { c->error = "unavailable"; break; }
        memcpy(out->valuestring, s, len + 1);
        out->type = cJSON_String;
        break;
    }
    case LUA_TTABLE: {
        const void *identity = lua_topointer(L, index);
        for (unsigned i = 0; i < depth; ++i) if (c->ancestors[i] == identity) { c->error = "invalid_value"; return; }
        c->ancestors[depth] = identity;
        size_t count = 0, largest = 0;
        bool strings = false, numbers = false, array = false;
        if (lua_getmetatable(L, index)) {
            lua_rawgetp(L, LUA_REGISTRYINDEX, &array_key);
            array = lua_rawequal(L, -1, -2);
            lua_pop(L, 2);
        }
        lua_pushnil(L);
        while (lua_next(L, index)) {
            ++count;
            if (lua_type(L, -2) == LUA_TSTRING) strings = true;
            else if (lua_isinteger(L, -2) && lua_tointeger(L, -2) > 0 &&
                     lua_tointeger(L, -2) <= MINIAPP_JSON_MAX_BYTES) {
                numbers = true;
                size_t n = (size_t)lua_tointeger(L, -2);
                if (n > largest) largest = n;
            } else c->error = "invalid_value";
            lua_pop(L, 1);
        }
        if ((strings && numbers) || (numbers && largest != count)) c->error = "invalid_value";
        if (c->error) break;
        array = numbers || (!count && array);
        out->type = array ? cJSON_Array : cJSON_Object;
        if (array) {
            for (size_t i = 1; i <= count && !c->error; ++i) {
                cJSON *item = cJSON_CreateNull();
                if (!item || !cJSON_AddItemToArray(out, item)) { cJSON_Delete(item); c->error = "unavailable"; break; }
                lua_rawgeti(L, index, i);
                fill_json(L, -1, item, c, depth + 1);
                lua_pop(L, 1);
            }
        } else {
            lua_pushnil(L);
            while (!c->error && lua_next(L, index)) {
                size_t n;
                const char *key = lua_tolstring(L, -2, &n);
                if (memchr(key, 0, n)) { c->error = "invalid_value"; break; }
                if (n > MINIAPP_JSON_MAX_BYTES - c->bytes) { c->error = "too_large"; break; }
                c->bytes += n;
                cJSON *item = cJSON_CreateNull();
                if (!item || !cJSON_AddItemToObject(out, key, item)) { cJSON_Delete(item); c->error = "unavailable"; break; }
                fill_json(L, -1, item, c, depth + 1);
                lua_pop(L, 1);
            }
        }
        break;
    }
    default: c->error = "invalid_value";
    }
}

static void push_json(lua_State *L, const cJSON *item, unsigned depth)
{
    if (!lua_checkstack(L, 6)) { luaL_error(L, "not enough Lua stack"); return; }
    if (cJSON_IsNull(item)) lua_pushlightuserdata(L, &null_key);
    else if (cJSON_IsBool(item)) lua_pushboolean(L, cJSON_IsTrue(item));
    else if (cJSON_IsNumber(item)) lua_pushnumber(L, item->valuedouble);
    else if (cJSON_IsString(item)) lua_pushstring(L, item->valuestring);
    else {
        lua_newtable(L);
        bool array = cJSON_IsArray(item);
        if (array) { lua_rawgetp(L, LUA_REGISTRYINDEX, &array_key); lua_setmetatable(L, -2); }
        int n = 1;
        for (const cJSON *child = item->child; child; child = child->next) {
            push_json(L, child, depth + 1);
            if (array) lua_rawseti(L, -2, n++);
            else lua_setfield(L, -2, child->string);
        }
    }
}

static bool valid_tree(const cJSON *item, unsigned depth, json_call_t *c)
{
    if (depth > MINIAPP_JSON_MAX_DEPTH) { c->error = "too_deep"; return false; }
    if (cJSON_IsNumber(item) && !isfinite(item->valuedouble)) { c->error = "invalid_json"; return false; }
    for (const cJSON *child = item->child; child; child = child->next) if (!valid_tree(child, depth + 1, c)) return false;
    return true;
}

static int json_protected(lua_State *L)
{
    json_call_t *c = lua_touserdata(L, 1);
    if (lua_toboolean(L, 3)) {
        size_t length;
        if (lua_type(L, 2) != LUA_TSTRING) { c->error = "invalid_json"; return 0; }
        const char *text = lua_tolstring(L, 2, &length);
        if (memchr(text, 0, length)) { c->error = "invalid_json"; return 0; }
        if (length > MINIAPP_JSON_MAX_BYTES || length > MINIAPP_LUA_HEAP_LIMIT) { c->error = "too_large"; return 0; }
        /* Bound parser recursion before allocating cJSON nodes. Reject escaped
         * NUL explicitly: cJSON strings cannot represent embedded NUL bytes. */
        unsigned depth = 0;
        bool quoted = false;
        for (size_t i = 0; i < length; ++i) {
            if (quoted && text[i] == '\\') {
                if (length - i >= 6 && !memcmp(text + i, "\\u0000", 6)) { c->error = "invalid_json"; return 0; }
                ++i;
            } else if (text[i] == '"') quoted = !quoted;
            else if (!quoted && (text[i] == '[' || text[i] == '{')) {
                if (++depth > MINIAPP_JSON_MAX_DEPTH + 1) { c->error = "too_deep"; return 0; }
            } else if (!quoted && (text[i] == ']' || text[i] == '}') && depth) --depth;
        }
        const char *end = NULL;
        c->root = cJSON_ParseWithLengthOpts(text, length + 1, &end, true);
        if (!c->root) { c->error = "invalid_json"; return 0; }
        if (valid_tree(c->root, 0, c)) { push_json(L, c->root, 0); return 1; }
    } else {
        c->root = cJSON_CreateNull();
        if (!c->root) { c->error = "unavailable"; return 0; }
        fill_json(L, 2, c->root, c, 0);
        if (c->error) return 0;
        /* cJSON documents five bytes of extra space for preallocated output. */
        size_t capacity = c->bytes * 6 + 6;
        if (capacity > MINIAPP_JSON_MAX_BYTES + 6) capacity = MINIAPP_JSON_MAX_BYTES + 6;
        c->printed = lisa_mem_alloc(capacity);
        if (!c->printed) { c->error = "unavailable"; return 0; }
        if (!cJSON_PrintPreallocated(c->root, c->printed, capacity, false) ||
            strlen(c->printed) > MINIAPP_JSON_MAX_BYTES || strlen(c->printed) > MINIAPP_LUA_HEAP_LIMIT) {
            c->error = "too_large"; return 0;
        }
        lua_pushstring(L, c->printed);
        return 1;
    }
    return 0;
}

static int json_call(lua_State *L, bool decode)
{
    json_call_t c = {0};
    lua_pushcfunction(L, json_protected);
    lua_pushlightuserdata(L, &c);
    lua_pushvalue(L, 1);
    lua_pushboolean(L, decode);
    int result = lua_pcall(L, 3, 1, 0);
    cJSON_Delete(c.root);
    lisa_mem_free(c.printed);
    if (result != LUA_OK) return lua_error(L);
    if (c.error) { lua_pop(L, 1); lua_pushnil(L); lua_pushstring(L, c.error); return 2; }
    return 1;
}
static int json_encode(lua_State *L) { return json_call(L, false); }
static int json_decode(lua_State *L) { return json_call(L, true); }

static void miniapp_json_open(lua_State *L)
{
    lua_newtable(L);
    lua_pushliteral(L, "JSON array"); lua_setfield(L, -2, "__metatable");
    lua_rawsetp(L, LUA_REGISTRYINDEX, &array_key);
    lua_newtable(L);
    lua_pushlightuserdata(L, &null_key); lua_setfield(L, -2, "null");
    lua_pushcfunction(L, json_encode); lua_setfield(L, -2, "encode");
    lua_pushcfunction(L, json_decode); lua_setfield(L, -2, "decode");
    lua_setglobal(L, "json");
}

static void miniapp_lua_http_response(lua_State *L, miniapp_http_response_t *response)
{
    lua_pushinteger(L, response->request->id);
    lua_newtable(L);
    if (response->error) {
        lua_newtable(L);
        lua_pushstring(L, response->error_code[0] ? response->error_code : "network"); lua_setfield(L, -2, "code");
        lua_pushstring(L, response->error_message[0] ? response->error_message : "缃戠粶璇锋眰澶辫触"); lua_setfield(L, -2, "message");
        lua_setfield(L, -2, "error");
    } else {
        lua_pushinteger(L, response->status); lua_setfield(L, -2, "status");
        lua_pushstring(L, response->content_type); lua_setfield(L, -2, "content_type");
        lua_pushlstring(L, response->body ? response->body : "", response->body_len); lua_setfield(L, -2, "body");
    }
}

static void miniapp_register_function(lua_State *L, const char *table,
                                      const char *name, lua_CFunction function)
{
    lua_getglobal(L, table);
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

static void miniapp_open_safe_libraries(lua_State *L)
{
    static const struct {
        const char *name;
        lua_CFunction open;
    } libs[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
    };

    for (size_t i = 0; i < sizeof(libs) / sizeof(libs[0]); ++i) {
        luaL_requiref(L, libs[i].name, libs[i].open, 1);
        lua_pop(L, 1);
    }

    const char *forbidden[] = {
        "collectgarbage", "dofile", "load", "loadfile", "print", "warn",
        "pcall", "xpcall", "setmetatable"
    };
    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        lua_pushnil(L);
        lua_setglobal(L, forbidden[i]);
    }

#if MINIAPP_HAS_SCREEN
    lua_newtable(L);
    lua_setglobal(L, "screen");
    miniapp_register_function(L, "screen", "begin", miniapp_lua_screen_begin);
    miniapp_register_function(L, "screen", "rect", miniapp_lua_screen_rect);
    miniapp_register_function(L, "screen", "text", miniapp_lua_screen_text);
    miniapp_register_function(L, "screen", "present", miniapp_lua_screen_present);
#endif

#if MINIAPP_HAS_LEDS
    lua_newtable(L);
    lua_setglobal(L, "led");
    miniapp_register_function(L, "led", "on", miniapp_lua_led_on);
    miniapp_register_function(L, "led", "off", miniapp_lua_led_off);
    miniapp_register_function(L, "led", "blink", miniapp_lua_led_blink);
#endif

#if MINIAPP_HAS_BUZZER
    lua_newtable(L);
    lua_setglobal(L, "buzzer");
    miniapp_register_function(L, "buzzer", "play", miniapp_lua_buzzer_play);
    miniapp_register_function(L, "buzzer", "play_seq", miniapp_lua_buzzer_play_seq);
#endif

    lua_newtable(L);
    lua_setglobal(L, "clock");
    miniapp_register_function(L, "clock", "now", miniapp_lua_clock_now);
    miniapp_register_function(L, "clock", "localtime", miniapp_lua_clock_localtime);
    lua_newtable(L);
    lua_setglobal(L, "storage");
    miniapp_register_function(L, "storage", "load", miniapp_lua_storage_load);
    miniapp_register_function(L, "storage", "save", miniapp_lua_storage_save);
    miniapp_register_function(L, "storage", "clear", miniapp_lua_storage_clear);

    lua_newtable(L);
    lua_setglobal(L, "http");
    miniapp_register_function(L, "http", "request", miniapp_lua_http_request);
    miniapp_register_function(L, "http", "get", miniapp_lua_http_get);
    miniapp_register_function(L, "http", "cancel", miniapp_lua_http_cancel);

    miniapp_json_open(L);

    lua_newtable(L);
    lua_setglobal(L, "tts");
    miniapp_register_function(L, "tts", "speak", miniapp_lua_tts_speak);
    miniapp_register_function(L, "tts", "cancel", miniapp_lua_tts_cancel);

    lua_newtable(L);
#if MINIAPP_HAS_SCREEN
    lua_pushinteger(L, MINIAPP_SCREEN_WIDTH);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, MINIAPP_SCREEN_HEIGHT);
    lua_setfield(L, -2, "height");
#endif
    lua_pushinteger(L, MINIAPP_API_VERSION);
    lua_setfield(L, -2, "api_version");
    lua_setglobal(L, "app");
}

static void miniapp_hook(lua_State *L, lua_Debug *ar)
{
    (void)ar;
    miniapp_vm_t *vm = miniapp_vm_get(L);
    TickType_t now = xTaskGetTickCount();
    if (vm->instruction_left <= MINIAPP_HOOK_GRANULARITY) {
        luaL_error(L, "instruction budget exceeded");
        return;
    }
    if ((int32_t)(now - vm->deadline) >= 0) {
#if configGENERATE_RUN_TIME_STATS && defined(SOC_TIMER_FREQ)
        /* A radio calibration can preempt this task for hundreds of ms.
         * FreeRTOS accounts completed running slices, excluding other tasks.
         * The current slice may be incomplete; the instruction limit remains
         * the hard bound even when no context switch updates this counter. */
        uint32_t cpu = (uint32_t)ulTaskGetRunTimeCounter(NULL) - vm->cpu_started;
        if (cpu < vm->cpu_limit) {
            vm->instruction_left -= MINIAPP_HOOK_GRANULARITY;
            return;
        }
#endif
        luaL_error(L, "wall-time budget exceeded");
        return;
    }
    vm->instruction_left -= MINIAPP_HOOK_GRANULARITY;
}

static void miniapp_set_deadline(miniapp_vm_t *vm, uint32_t milliseconds)
{
    vm->deadline = xTaskGetTickCount() + pdMS_TO_TICKS(milliseconds);
#if configGENERATE_RUN_TIME_STATS && defined(SOC_TIMER_FREQ)
    vm->cpu_started = (uint32_t)ulTaskGetRunTimeCounter(NULL);
    vm->cpu_limit = (uint32_t)((uint64_t)milliseconds * SOC_TIMER_FREQ /
                             1000u / (1000u / configTICK_RATE_HZ));
#endif
}

static int miniapp_open_libraries_protected(lua_State *L)
{
    miniapp_open_safe_libraries(L);
    return 0;
}

static lua_State *miniapp_vm_create(miniapp_vm_t *vm, bool validating)
{
    memset(vm, 0, sizeof(*vm));
    vm->limit = MINIAPP_LUA_HEAP_LIMIT;
    vm->http_generation = ++s_http_generation;
    vm->validating = validating;
    vm->pending_led = -1;
    vm->scene.background = 0;
    lua_State *L = lua_newstate(miniapp_lua_alloc, vm);
    if (!L) {
        return NULL;
    }
    *(miniapp_vm_t **)lua_getextraspace(L) = vm;
    lua_pushcfunction(L, miniapp_open_libraries_protected);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        lua_close(L);
        return NULL;
    }
    lua_sethook(L, miniapp_hook, LUA_MASKCOUNT, MINIAPP_HOOK_GRANULARITY);
    return L;
}

static int miniapp_run_chunk(lua_State *L, miniapp_vm_t *vm,
                             const char *source, size_t source_len,
                             char *error, size_t error_size)
{
    TickType_t started = xTaskGetTickCount();
    vm->instruction_left = MINIAPP_CHUNK_INSTRUCTION_LIMIT;
    miniapp_set_deadline(vm, MINIAPP_CHUNK_DEADLINE_MS);
    int rc = luaL_loadbufferx(L, source, source_len, "@cloud-miniapp", "t");
    if (rc == LUA_OK) {
        rc = lua_pcall(L, 0, 0, 0);
    }
    /* 记录真实耗时：源码块的预算按解析算，只有量出来才知道 64 KiB 上限下
     * 离 MINIAPP_CHUNK_DEADLINE_MS 还有多少余量。 */
    LISA_LOGI(TAG, "chunk %u bytes: load+run %u ms (budget %u ms)",
              (unsigned)source_len,
              (unsigned)((xTaskGetTickCount() - started) * portTICK_PERIOD_MS),
              (unsigned)MINIAPP_CHUNK_DEADLINE_MS);
    if (rc != LUA_OK) {
        miniapp_storage_fault(vm);
        snprintf(error, error_size, "%s", lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "Lua error");
        lua_pop(L, 1);
        return -1;
    }
    return 0;
}

typedef struct {
    const char *name;
    const char *button;
    miniapp_http_response_t *http_response;
    lua_Integer argument;
    const char *text_argument;
    const char *error_code;
    bool result_table;
    int nargs;
} miniapp_callback_t;

static int miniapp_callback_protected(lua_State *L)
{
    miniapp_callback_t *call = lua_touserdata(L, 1);
    lua_getglobal(L, call->name);
    if (lua_isnil(L, -1) && strcmp(call->name, "on_tick") != 0) {
        return 0;
    }
    if (!lua_isfunction(L, -1)) {
        return luaL_error(L, "%s must be a function", call->name);
    }
    if (call->http_response) miniapp_lua_http_response(L, call->http_response);
    else if (call->button) lua_pushstring(L, call->button);
    else if (call->result_table) {
        lua_pushinteger(L, call->argument);
        lua_newtable(L);
        lua_pushstring(L, call->text_argument);
        lua_setfield(L, -2, "status");
        if (call->error_code) {
            lua_newtable(L);
            lua_pushstring(L, call->error_code);
            lua_setfield(L, -2, "code");
            lua_pushstring(L, call->error_code);
            lua_setfield(L, -2, "message");
            lua_setfield(L, -2, "error");
        }
    } else if (call->text_argument) {
        lua_pushinteger(L, call->argument);
        lua_pushstring(L, call->text_argument);
    } else if (call->nargs) lua_pushinteger(L, call->argument);
    lua_call(L, call->nargs, 0);
    return 0;
}

static int miniapp_invoke(lua_State *L, miniapp_vm_t *vm, miniapp_callback_t *call)
{
    vm->instruction_left = MINIAPP_CALLBACK_INSTRUCTION_LIMIT;
    TickType_t started = xTaskGetTickCount();
    miniapp_set_deadline(vm, MINIAPP_CALLBACK_DEADLINE_MS);
    /* Even callback lookup and string arguments may allocate. Keep them under
     * pcall so an exhausted heap cannot invoke Lua's process-wide panic. */
    lua_pushcfunction(L, miniapp_callback_protected);
    lua_pushlightuserdata(L, call);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        miniapp_storage_fault(vm);
        miniapp_set_error(lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "Lua error");
        lua_pop(L, 1);
        return -1;
    }
    uint32_t elapsed = (uint32_t)((xTaskGetTickCount() - started) * portTICK_PERIOD_MS);
    s_status.last_frame_ms = elapsed;
    if (elapsed > s_status.max_frame_ms) s_status.max_frame_ms = elapsed;
    return 0;
}

static int miniapp_call(lua_State *L, miniapp_vm_t *vm, const char *name,
                        int nargs, lua_Integer argument)
{
    miniapp_callback_t call = {.name = name, .nargs = nargs, .argument = argument};
    return miniapp_invoke(L, vm, &call);
}

static int miniapp_call_button(lua_State *L, miniapp_vm_t *vm, const char *button_id)
{
    miniapp_callback_t call = {.name = "on_button_click", .nargs = 1, .button = button_id};
    return miniapp_invoke(L, vm, &call);
}

static int miniapp_call_http_response(lua_State *L, miniapp_vm_t *vm,
                                      miniapp_http_response_t *response)
{
    miniapp_callback_t call = {.name = "on_http_response", .nargs = 2,
                               .http_response = response};
    /* Release admission quota before Lua can chain its next request. */
    miniapp_http_remove(response->request);
    int result = miniapp_invoke(L, vm, &call);
    miniapp_http_free(response->request, response);
    return result;
}

static int miniapp_call_tts_result(lua_State *L, miniapp_vm_t *vm,
                                   uint32_t speech_id, const char *result, const char *error)
{
    LISA_LOGI(TAG, "TTS result id=%u status=%s error=%s", (unsigned)speech_id, result, error ? error : "");
    miniapp_callback_t call = {
        .name = "on_tts_result",
        .nargs = 2,
        .argument = speech_id,
        .text_argument = result,
        .error_code = error,
        .result_table = true,
    };
    return miniapp_invoke(L, vm, &call);
}

static void miniapp_tts_stop(void)
{
    uint32_t owner = 0;
    taskENTER_CRITICAL();
    for (unsigned i = 0; i < MINIAPP_TTS_RESULT_SLOTS; ++i) {
        miniapp_tts_job_t *job = s_tts_requests[i];
        if (job && job->generation == s_buzzer_generation) {
            job->cancelled = true;
            if (job->speech_id == s_tts_worker_id) owner = job->speech_id;
        }
    }
    taskEXIT_CRITICAL();
    if (owner) voice_player_tts_cancel_owned(owner);
}

static void miniapp_runtime_stop(lua_State **state, miniapp_vm_t *vm, bool navigate_home)
{
    miniapp_http_discard(vm);
    miniapp_tts_stop();
    if (*state) {
        (void)miniapp_call(*state, vm, "on_exit", 0, 0);
        lua_close(*state);
        *state = NULL;
    }
    ++s_buzzer_generation;
    service_led_off();
    miniapp_set_active(false);
#ifdef CONFIG_MINIAPP_ADB_DEBUG
    miniapp_source_replace(NULL);
#endif
    taskENTER_CRITICAL();
    memset(&s_current, 0, sizeof(s_current));
    taskEXIT_CRITICAL();
    s_status.installed = false;
    if (s_buzzer_queue) {
        xQueueReset(s_buzzer_queue);
    }
    if (navigate_home) {
        (void)miniapp_ui_close();
    }
    LISA_LOGI(TAG, "runtime stopped");
}

/* Only the Lua task owns VM lifetime. The previous VM stays intact until the
 * candidate has loaded, executed startup callbacks and reached the UI thread. */
static int miniapp_replace(lua_State **current, miniapp_vm_t **current_vm,
                           miniapp_request_t *request)
{
    miniapp_vm_t *candidate = lisa_mem_calloc(1, sizeof(*candidate));
    lua_State *next = candidate ? miniapp_vm_create(candidate, true) : NULL;
#if MINIAPP_HAS_SCREEN
    bool had_current = *current != NULL;
#endif
    if (!next) {
        snprintf(request->error, sizeof(request->error), "not enough memory for Lua VM");
        lisa_mem_free(candidate);
        return -1;
    }
#ifdef CONFIG_MINIAPP_ADB_DEBUG
    miniapp_source_t *source = lisa_mem_alloc(sizeof(*source) + request->package->size);
    if (!source) {
        snprintf(request->error, sizeof(request->error), "not enough memory for source snapshot");
        goto failed;
    }
    source->refs = 1;
    source->size = request->package->size;
    memcpy(source->data, request->source, source->size);
#endif
    snprintf(candidate->id, sizeof(candidate->id), "%s", request->package->id);
    if (miniapp_run_chunk(next, candidate, request->source, request->package->size,
                         request->error, sizeof(request->error)) != 0) {
        goto failed;
    }
    if (miniapp_call(next, candidate, "on_start", 0, 0) != 0 ||
        miniapp_call(next, candidate, "on_tick", 1, MINIAPP_TICK_MS) != 0) {
        snprintf(request->error, sizeof(request->error), "%s", s_status.last_error);
        goto failed;
    }
#if MINIAPP_HAS_SCREEN
    if (!candidate->present_count) {
        snprintf(request->error, sizeof(request->error), "startup must present a frame");
        goto failed;
    }
    if (miniapp_ui_present(&candidate->presented_scene) != 0 || miniapp_ui_open() != 0 ||
        miniapp_ui_wait_frame(2000) != 0) {
        snprintf(request->error, sizeof(request->error), "failed to show miniapp frame");
        if (had_current) {
            (void)miniapp_ui_present(&(*current_vm)->presented_scene);
        } else {
            (void)miniapp_ui_close();
        }
        goto failed;
    }
#endif
    /* The candidate is now usable. Do not run old callbacks with device side
     * effects: they must not clear the new scene or enqueue old sounds. */
    if (*current) {
        miniapp_tts_stop();
        (*current_vm)->validating = true;
        (*current_vm)->retiring = true;
        (void)miniapp_call(*current, *current_vm, "on_exit", 0, 0);
        miniapp_http_discard(*current_vm);
        lua_close(*current);
        lisa_mem_free(*current_vm);
    }
    *current = next;
    *current_vm = candidate;
    candidate->validating = false;
    miniapp_http_activate(candidate);
    if (candidate->storage_pending) {
        const char *error = candidate->storage_pending == 2 ? miniapp_storage_clear(candidate->id) :
            miniapp_storage_save(candidate->id, candidate->storage_data, candidate->storage_size, candidate->storage_ttl);
        if (error) LISA_LOGW(TAG, "startup app data not saved: %s", error);
        candidate->storage_pending = 0;
    }
    ++s_buzzer_generation;
    if (s_buzzer_queue) {
        xQueueReset(s_buzzer_queue);
    }
    service_led_off();
    taskENTER_CRITICAL();
    s_current = *request->package;
    taskEXIT_CRITICAL();
    memset(&s_status, 0, sizeof(s_status));
    s_status.installed = true;
    s_status.source_size = request->package->size;
#ifdef CONFIG_MINIAPP_ADB_DEBUG
    miniapp_source_replace(source);
#endif
    miniapp_set_active(true);
    if (candidate->pending_led == 1) service_led_on();
    if (candidate->pending_led == 2) service_led_blink(candidate->led_on_ms, candidate->led_off_ms);
    for (unsigned i = 0; i < candidate->buzz_count; ++i) {
        candidate->buzz[i].generation = s_buzzer_generation;
        miniapp_buzzer_enqueue(&candidate->buzz[i]);
    }
    if (voice_intent_contains(INTENT_MUSIC)) {
        voice_player_music_publish_info(NULL, NULL);
        (void)voice_intent_pop(INTENT_MUSIC);
    } else if (music_player && app_player_get_state(music_player) != APP_PLAYER_STATE_IDLE) {
        (void)app_player_stop(music_player);
    }
    return 0;
failed:
#ifdef CONFIG_MINIAPP_ADB_DEBUG
    miniapp_source_release(source);
#endif
    miniapp_http_discard(candidate);
    lua_close(next);
    lisa_mem_free(candidate);
    return -1;
}

static void miniapp_task(void *argument)
{
    (void)argument;
    lua_State *L = NULL;
    miniapp_vm_t *vm = NULL;
    TickType_t next_tick = xTaskGetTickCount();
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        TickType_t wait = s_active ? ((int32_t)(next_tick - now) > 0 ? next_tick - now : 0)
                                  : pdMS_TO_TICKS(MINIAPP_TICK_MS);
        miniapp_event_t event;
        if (xQueueReceive(s_event_queue, &event, wait) == pdTRUE) {
            if (event.type == MINIAPP_EVENT_INSTALL) {
                miniapp_request_t *request = event.request;
                if (event.generation != s_buzzer_generation || alarm_ring_is_active()) {
                    snprintf(request->error, sizeof(request->error), "application changed or alarm is ringing");
                    request->result = -1;
                } else {
                    request->result = miniapp_replace(&L, &vm, request);
                }
                next_tick = xTaskGetTickCount() + pdMS_TO_TICKS(MINIAPP_TICK_MS);
                xSemaphoreGive(request->done);
                /* request belongs to the waiting caller; never touch it again. */
            } else if (event.generation != s_buzzer_generation) {
                /* Input queued for a previous instance cannot affect its replacement. */
                continue;
            } else if (event.type == MINIAPP_EVENT_CLICK && L && !alarm_ring_is_active()) {
                if (miniapp_call_button(L, vm, event.button_id) != 0) {
                    miniapp_runtime_stop(&L, vm, true);
                    lisa_mem_free(vm);
                    vm = NULL;
                }
            } else if (event.type == MINIAPP_EVENT_EXIT && L) {
                miniapp_runtime_stop(&L, vm, true);
                lisa_mem_free(vm);
                vm = NULL;

            }
        }
        miniapp_http_response_t *response;
        for (unsigned i = 0; i < MINIAPP_HTTP_MAX_REQUESTS &&
             (response = miniapp_http_take_result(vm, alarm_ring_is_active())) != NULL; ++i) {
            if (L && vm && response->request->generation == vm->http_generation &&
                !miniapp_http_is_cancelled(response->request)) {
                if (miniapp_call_http_response(L, vm, response) != 0) {
                    miniapp_runtime_stop(&L, vm, true);
                    lisa_mem_free(vm);
                    vm = NULL;
                }
            } else miniapp_http_free(response->request, response);
        }
        miniapp_tts_job_t *tts_result;
        /* A callback may immediately submit another busy request. Bound each
         * drain so that callbacks cannot starve ticks or the exit event. */
        for (unsigned i = 0; i < MINIAPP_TTS_RESULT_SLOTS &&
             (tts_result = miniapp_tts_take_result(alarm_ring_is_active())) != NULL; ++i) {
            if (L && !tts_result->cancelled && tts_result->generation == s_buzzer_generation &&
                miniapp_call_tts_result(L, vm, tts_result->speech_id,
                                       tts_result->result, tts_result->error) != 0) {
                miniapp_runtime_stop(&L, vm, true);
                lisa_mem_free(vm);
                vm = NULL;
            }
            lisa_mem_free(tts_result);
        }
        /* Events must not starve ticks when the input queue remains busy. */
        now = xTaskGetTickCount();
        if (L && (int32_t)(now - next_tick) >= 0) {
            if (alarm_ring_is_active()) {
                next_tick = now + pdMS_TO_TICKS(MINIAPP_TICK_MS);
                continue;
            }
            if (miniapp_call(L, vm, "on_tick", 1, MINIAPP_TICK_MS) != 0) {
                miniapp_runtime_stop(&L, vm, true);
                lisa_mem_free(vm);
                vm = NULL;
            } else {
                s_status.frames++;
            }
            next_tick = xTaskGetTickCount() + pdMS_TO_TICKS(MINIAPP_TICK_MS);
        }
    }
}

int miniapp_init(void)
{
    if (s_task) {
        return 0;
    }
    s_event_queue = xQueueCreate(8, sizeof(miniapp_event_t));
    s_http_queue = xQueueCreate(MINIAPP_HTTP_MAX_REQUESTS, sizeof(miniapp_http_request_t *));
    s_http_lock = xSemaphoreCreateMutex();
#if MINIAPP_HAS_BUZZER
    s_buzzer_queue = xQueueCreate(4, sizeof(miniapp_buzzer_command_t));
    if (!s_buzzer_queue) {
        goto failed;
    }
#endif
    if (!s_event_queue || !s_http_queue || !s_http_lock || miniapp_ui_init() != 0) {
        goto failed;
    }
    if (xTaskCreate(miniapp_http_task, "miniapp.http", 4096, NULL, 4,
                    &s_http_task) != pdPASS) {
        goto failed;
    }

#if MINIAPP_HAS_BUZZER
    if (xTaskCreate(miniapp_buzzer_task, "miniapp.buzz", 2048, NULL, 4,
                    &s_buzzer_task) != pdPASS) {
        goto failed;
    }
#endif
    if (xTaskCreate(miniapp_task, "miniapp.lua", 8192, NULL, 5, &s_task) != pdPASS) {
        goto failed;
    }
    return 0;
failed:
    if (s_http_task) {
        vTaskDelete(s_http_task);
        s_http_task = NULL;
    }
    if (s_http_lock) {
        vSemaphoreDelete(s_http_lock);
        s_http_lock = NULL;
    }
    if (s_http_queue) {
        vQueueDelete(s_http_queue);
        s_http_queue = NULL;
    }
    if (s_buzzer_task) {
        vTaskDelete(s_buzzer_task);
        s_buzzer_task = NULL;
    }
    if (s_buzzer_queue) {
        vQueueDelete(s_buzzer_queue);
        s_buzzer_queue = NULL;
    }
    if (s_event_queue) {
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
    }
    return -1;
}

bool miniapp_is_active(void) { return s_active; }

const char *lsc_get_nlu_custom_mode(void)
{
    return miniapp_is_active() ? "miniapp" : NULL;
}

bool lsc_get_nlu_custom_miniapp(char *id, size_t id_size,
                                char *version, size_t version_size)
{
    bool active;

    if (!id || !id_size || !version || !version_size) return false;
    id[0] = '\0';
    version[0] = '\0';
    taskENTER_CRITICAL();
    active = s_active && s_current.id[0] && s_current.version[0];
    if (active) {
        snprintf(id, id_size, "%s", s_current.id);
        snprintf(version, version_size, "%s", s_current.version);
    }
    taskEXIT_CRITICAL();
    return active;
}

bool miniapp_install_begin(void)
{
    bool accepted;
    taskENTER_CRITICAL();
    accepted = s_task && !s_install_busy;
    if (accepted) {
        s_install_busy = true;
        s_install_generation = s_buzzer_generation;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

void miniapp_install_end(void)
{
    taskENTER_CRITICAL();
    s_install_busy = false;
    taskEXIT_CRITICAL();
}

bool miniapp_matches(const miniapp_package_t *package)
{
    bool match;
    taskENTER_CRITICAL();
    match = s_active && s_current.size == package->size &&
            strcmp(s_current.id, package->id) == 0 &&
            strcmp(s_current.version, package->version) == 0 &&
            strcmp(s_current.hash, package->hash) == 0;
    taskEXIT_CRITICAL();
    return match;
}

int miniapp_install(const miniapp_package_t *package, const char *source,
                    char *error, size_t error_size)
{
    if (!s_task || !package || !source || !error || !error_size ||
        package->size == 0 || package->size > MINIAPP_SOURCE_MAX ||
        memchr(source, '\0', package->size)) {
        if (error && error_size) {
            snprintf(error, error_size, "invalid Lua source or runtime unavailable");
        }
        return -1;
    }
    miniapp_request_t request = {.package = package, .source = source, .result = -1};
    request.done = xSemaphoreCreateBinary();
    if (!request.done) {
        snprintf(error, error_size, "not enough memory for install request");
        return -1;
    }
    miniapp_event_t event = {.type = MINIAPP_EVENT_INSTALL, .request = &request,
                             .generation = s_install_generation};
    if (xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
        vSemaphoreDelete(request.done);
        snprintf(error, error_size, "miniapp queue is busy");
        return -1;
    }
    /* The MCP worker owns these buffers until the bounded Lua/UI operations
     * finish. A caller timeout must never free an in-flight request. */
    xSemaphoreTake(request.done, portMAX_DELAY);
    vSemaphoreDelete(request.done);
    snprintf(error, error_size, "%s", request.error);
    return request.result;
}

/* 允许转发给脚本的按键 id 白名单。设备侧只放行单击与双击; 三击及以上、
 * 长按等动作由 app_button.c 在转发前就丢掉, 这里再兜一层, 防止将来新增
 * 转发点时把未上报给能力的 id 悄悄送进脚本。 */
static bool miniapp_button_id_supported(const char *button_id)
{
    /* 设备功能键 + 手柄按键。新增取值时必须同时加到 miniapp.h 的宏和这里，
     * 漏加的表现是"手柄按了没反应且日志里什么都没有"（见下面的告警日志兜底）。 */
    static const char *const supported[] = {
        MINIAPP_BUTTON_ID_FUNCTION,
        MINIAPP_BUTTON_ID_FUNCTION_DOUBLE,
        MINIAPP_BUTTON_ID_UP,
        MINIAPP_BUTTON_ID_DOWN,
        MINIAPP_BUTTON_ID_LEFT,
        MINIAPP_BUTTON_ID_RIGHT,
        MINIAPP_BUTTON_ID_BACK,
        MINIAPP_BUTTON_ID_SETTINGS,
    };

    for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i) {
        if (strcmp(button_id, supported[i]) == 0) {
            return true;
        }
    }
    return false;
}

int miniapp_button_click(const char *button_id)
{
    if (!MINIAPP_HAS_BUTTONS) return 0;
    if (!s_active || !button_id) {
        return -1;
    }
    if (strlen(button_id) > MINIAPP_BUTTON_ID_MAX) {
        LISA_LOGW(TAG, "button id too long: %s", button_id);
        return -1;
    }
    if (!miniapp_button_id_supported(button_id)) {
        /* 不静默丢弃: 漏配白名单的表现就是"按键没反应"，只有这条日志能说明
         * "事件确实进来了、是被固件拒的"，而不是上游根本没发。 */
        LISA_LOGW(TAG, "unsupported button id: %s", button_id);
        return -1;
    }
    miniapp_event_t event = {.type = MINIAPP_EVENT_CLICK, .generation = s_buzzer_generation};
    snprintf(event.button_id, sizeof(event.button_id), "%s", button_id);
    int rc = xQueueSend(s_event_queue, &event, 0) == pdTRUE ? 0 : -1;
    /* 这是"外部输入进入小应用沙箱"的唯一观测点：按键驱动聚合多击、回调失败即
     * 静默丢弃，没有这行日志就无法判断双击到底有没有被转发进来。 */
    LISA_LOGI(TAG, "button %s -> %s", button_id, rc == 0 ? "queued" : "dropped");
    return rc;
}

int miniapp_exit(void)
{
    if (!s_active) {
        return -1;
    }
    miniapp_event_t event = {.type = MINIAPP_EVENT_EXIT, .generation = s_buzzer_generation};
    return xQueueSendToFront(s_event_queue, &event, 0) == pdTRUE ? 0 : -1;
}

void miniapp_runtime_ui_closed(void)
{
    if (s_active) {
        (void)miniapp_exit();
    }
}
