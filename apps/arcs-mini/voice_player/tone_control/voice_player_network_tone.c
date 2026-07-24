#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#define TAG "voice_network_tone"
#include "lisa_log.h"

#include "app_datas.h"
#include "app_tone.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"
#include "tone.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "voice_player/voice_player_tts.h"

#ifdef CONFIG_OTA
#include "ota_manager.h"
#endif

#include "tone_control/voice_player_network_tone.h"
#include "tone_control/voice_player_tone.h"

/*
 * 网络提示音策略：将链路、探测、云连接和配网消息收敛为两个状态机，
 * 分别控制断网提示与云恢复提示，避免同一网络波动重复播报。
 *
 * 断网提示状态：
 *   CLEAR -> GRACE：发现链路仍在、但网络暂时不可用，等待短暂宽限期。
 *   GRACE -> ANNOUNCED：宽限期结束后仍不可用，播放“网络不稳定”。
 *   GRACE -> CLEAR：宽限期内恢复，不播放任何提示音。
 *   CLEAR -> ANNOUNCED：物理链路直接断开，立即播放“网络已断开”。
 *   ANNOUNCED -> CLEAR：网络探测恢复后播放一次“网络连接成功”。
 *
 * 云恢复提示状态：
 *   IDLE -> RECONNECT_PENDING：云连接断开，等待真正的重连事件。
 *   RECONNECT_PENDING -> SUCCESS_DELAYED：云连接恢复，延迟播放成功提示。
 *   SUCCESS_DELAYED -> SUCCESS_ANNOUNCED：延迟期内没有更高优先级事件，播放成功提示。
 *   Pushup TTS、绑定消息、鉴权失败或再次断线会取消尚未播放的成功提示。
 */

/* ==================== 私有配置、类型与状态 ==================== */

/* 云端 OPEN_INFO 消息中，状态 3 表示需要展示 BLE 绑定信息。 */
#define CLOUD_OPEN_INFO_STATUS_BIND 3U

/* 延迟窗口用于过滤网络抖动，限频窗口用于过滤重复消息。 */
#define CLOUD_SUCCESS_TONE_DELAY_MS 2000U
#define NETWORK_UNSTABLE_TONE_GRACE_MS 800U
#define NETWORK_SUCCESS_TONE_MIN_INTERVAL_MS 3000U
#define BIND_TONE_MIN_INTERVAL_MS 3000U

#define NETWORK_SUCCESS_TONE_ID TONE_ID_59
#define NETWORK_DISCONNECTED_TONE_ID TONE_ID_60
#define NETWORK_UNSTABLE_TONE_ID TONE_ID_65
#define NETCFG_ENTER_TONE_ID TONE_ID_70
#define NETCFG_EXIT_TONE_ID TONE_ID_71
#define NETCFG_SUCCESS_TONE_ID TONE_ID_72
#define CLOUD_AUTH_FAILED_TONE_ID TONE_ID_105

typedef enum {
    NETWORK_OUTAGE_CLEAR = 0, /* 当前没有待处理的断网提示。 */
    NETWORK_OUTAGE_GRACE,     /* 已发现异常，正在等待宽限期结束。 */
    NETWORK_OUTAGE_ANNOUNCED, /* 已提示用户，后续异常消息不再重复播报。 */
} network_outage_state_t;

typedef enum {
    CLOUD_RECOVERY_IDLE = 0,            /* 当前没有云重连流程。 */
    CLOUD_RECOVERY_RECONNECT_PENDING,   /* 云已断开，等待连接恢复。 */
    CLOUD_RECOVERY_SUCCESS_DELAYED,     /* 已恢复，成功提示正在延迟窗口中。 */
    CLOUD_RECOVERY_SUCCESS_ANNOUNCED,   /* 本轮恢复已经提示成功。 */
} cloud_recovery_state_t;

/* EBUS 回调和 FreeRTOS 定时器回调可能来自不同任务，状态统一由 lock 保护。 */
typedef struct {
    network_outage_state_t outage;
    cloud_recovery_state_t recovery;
    /* BLE 配网已有独立成功提示时，抑制紧随其后的云连接成功提示。 */
    bool suppress_next_success;
    /* 以下时间戳分别限制网络成功提示和绑定提示的重复播报。 */
    TickType_t last_network_success_tick;
    TickType_t last_bind_tick;
    /* 延迟成功提示，为云端 TTS 或绑定流程保留取消窗口。 */
    TimerHandle_t cloud_success_delay_timer;
    /* 过滤短暂断网和网络探测抖动。 */
    TimerHandle_t network_unstable_grace_timer;
    /* 只保护本结构中的策略状态，不覆盖实际音频播放过程。 */
    SemaphoreHandle_t lock;
} network_tone_context_t;

static network_tone_context_t s_network_tone;

/* ==================== 内部工具：状态与定时器 ==================== */

static bool network_tone_lock(void)
{
    if (s_network_tone.lock == NULL) {
        LOGW("network tone policy is not initialized");
        return false;
    }

    return xSemaphoreTake(s_network_tone.lock, portMAX_DELAY) == pdTRUE;
}

static void network_tone_unlock(void)
{
    xSemaphoreGive(s_network_tone.lock);
}

static void network_tone_play(uint16_t tone_id)
{
    voice_player_play_tone_url(app_tone_get_url(tone_id));
}

/* 名称以 _locked 结尾的函数要求调用方已经持有 s_network_tone.lock。 */
static void network_tone_stop_timer_locked(TimerHandle_t timer,
                                           const char *name)
{
    if (timer != NULL &&
        xTimerIsTimerActive(timer) != pdFALSE &&
        xTimerStop(timer, 0) != pdPASS) {
        LOGW("failed to stop %s timer", name);
    }
}

static void network_tone_cancel_unstable_grace_locked(void)
{
    network_tone_stop_timer_locked(s_network_tone.network_unstable_grace_timer,
                                   "network unstable grace");
    /* ANNOUNCED 必须保留，恢复事件据此判断是否需要补播成功提示。 */
    if (s_network_tone.outage == NETWORK_OUTAGE_GRACE) {
        s_network_tone.outage = NETWORK_OUTAGE_CLEAR;
    }
}

static void network_tone_clear_outage_locked(void)
{
    network_tone_stop_timer_locked(s_network_tone.network_unstable_grace_timer,
                                   "network unstable grace");
    s_network_tone.outage = NETWORK_OUTAGE_CLEAR;
}

static void network_tone_cancel_success_delay_locked(void)
{
    network_tone_stop_timer_locked(s_network_tone.cloud_success_delay_timer,
                                   "cloud success delay");
    /* 取消延迟提示不能丢失“仍在等待云重连”这一事实。 */
    if (s_network_tone.recovery != CLOUD_RECOVERY_RECONNECT_PENDING) {
        s_network_tone.recovery = CLOUD_RECOVERY_IDLE;
    }
}

static void network_tone_reset_locked(void)
{
    network_tone_clear_outage_locked();
    network_tone_stop_timer_locked(s_network_tone.cloud_success_delay_timer,
                                   "cloud success delay");
    s_network_tone.recovery = CLOUD_RECOVERY_IDLE;
}

static void network_tone_reset(void)
{
    if (!network_tone_lock()) {
        return;
    }

    network_tone_reset_locked();
    network_tone_unlock();
}

static void network_tone_cancel_success_delay(void)
{
    if (!network_tone_lock()) {
        return;
    }

    network_tone_cancel_success_delay_locked();
    network_tone_unlock();
}

static void network_tone_cancel_unstable_grace(void)
{
    if (!network_tone_lock()) {
        return;
    }

    network_tone_cancel_unstable_grace_locked();
    network_tone_unlock();
}

/* ==================== 内部工具：网络条件与提示音限频 ==================== */

/* 链路存在仅表示 Wi-Fi 已关联或 Modem 已接入，不代表互联网已经可用。 */
static bool network_status_has_link(const sys_network_status_t *status,
                                    bool status_ok)
{
    if (!status_ok) {
        return false;
    }

    if (status->active_bearer == SYS_NETWORK_BEARER_WIFI) {
        return status->wifi_connected;
    }

    if (status->active_bearer == SYS_NETWORK_BEARER_MODEM) {
        return status->modem_connected;
    }

    return false;
}

static bool network_status_is_unstable(const sys_network_status_t *status,
                                       bool status_ok)
{
    /* 物理链路仍在但上层网络不可用，归类为“不稳定”而不是“已断开”。 */
    return network_status_has_link(status, status_ok) && !status->connected;
}

static bool network_tone_ota_suppressed(void)
{
#ifdef CONFIG_OTA
    switch (ota_manager_get_state()) {
    case OTA_STATE_CHECKING:
    case OTA_STATE_PACKAGE_INFO:
    case OTA_STATE_UPDATING:
    case OTA_STATE_APP_FAILED:
    case OTA_STATE_RESOURCE_FAILED:
        return true;
    default:
        return false;
    }
#else
    return false;
#endif
}

static bool network_tone_skip_disconnect(const sys_network_status_t *status,
                                         bool status_ok)
{
    /* OTA、承载切换和无已保存 AP 都是预期状态，不应向用户报告网络故障。 */
    if (network_tone_ota_suppressed()) {
        network_tone_reset();
        LOGI("skip network disconnect tone during OTA");
        return true;
    }

    if (status_ok && status->switching) {
        network_tone_reset();
        LOGI("skip network disconnect tone while switching bearer");
        return true;
    }

    if (!sys_wifi_has_ap()) {
        network_tone_reset();
        LOGI("skip network disconnect tone because no saved AP");
        return true;
    }

    return false;
}

static bool network_tone_rate_limit_allows(TickType_t *last_tick,
                                           uint32_t interval_ms)
{
    TickType_t now = xTaskGetTickCount();

    /* 多个 EBUS 来源可能报告同一次恢复或绑定，按提示音类型独立限频。 */
    if (*last_tick != 0U &&
        (now - *last_tick) < pdMS_TO_TICKS(interval_ms)) {
        return false;
    }

    *last_tick = now;
    return true;
}

static void network_tone_play_success(void)
{
    bool should_play = false;

    if (!network_tone_lock()) {
        return;
    }

    should_play = network_tone_rate_limit_allows(
        &s_network_tone.last_network_success_tick,
        NETWORK_SUCCESS_TONE_MIN_INTERVAL_MS);
    network_tone_unlock();

    if (should_play) {
        network_tone_play(NETWORK_SUCCESS_TONE_ID);
    }
}

static void network_tone_play_bind(void)
{
    bool should_play = false;

    if (!network_tone_lock()) {
        return;
    }

    should_play = network_tone_rate_limit_allows(
        &s_network_tone.last_bind_tick,
        BIND_TONE_MIN_INTERVAL_MS);
    network_tone_unlock();

    if (should_play) {
        network_tone_play(NETCFG_EXIT_TONE_ID);
    }
}

static bool cloud_unbound_active(void)
{
    /* 云通道存在但业务连接失败时，仍处于未绑定或鉴权失败流程。 */
    return voice_cloud_is_connected() &&
           voice_cloud_get_state() == VOICE_CLOUD_STATE_CONNECT_FAILED;
}

/* ==================== 定时器回调与状态转换 ==================== */

static void network_tone_cloud_success_delay_timer_cb(TimerHandle_t timer)
{
    bool should_play = false;

    (void)timer;

    if (!network_tone_lock()) {
        return;
    }

    /* 状态仍为 DELAYED 才播放，说明期间没有 TTS、绑定或断线将其取消。 */
    if (s_network_tone.recovery == CLOUD_RECOVERY_SUCCESS_DELAYED) {
        s_network_tone.recovery = CLOUD_RECOVERY_SUCCESS_ANNOUNCED;
        should_play = true;
    }
    network_tone_unlock();

    if (should_play) {
        network_tone_play_success();
    }
}

static void network_tone_unstable_grace_timer_cb(TimerHandle_t timer)
{
    /* 事件携带的状态可能已经过期，宽限期结束时重新读取当前网络状态。 */
    sys_network_status_t status = {0};
    bool status_ok = sys_network_get_status(&status) == 0;
    bool should_play = false;

    (void)timer;

    if (!network_tone_lock()) {
        return;
    }

    /* 状态不再是 GRACE，说明恢复消息或明确断线消息已经处理过该异常。 */
    if (s_network_tone.outage != NETWORK_OUTAGE_GRACE) {
        network_tone_unlock();
        return;
    }

    if (network_tone_ota_suppressed() ||
        (status_ok && status.switching) ||
        !sys_wifi_has_ap()) {
        network_tone_reset_locked();
    } else if (network_status_is_unstable(&status, status_ok)) {
        s_network_tone.outage = NETWORK_OUTAGE_ANNOUNCED;
        should_play = true;
    } else {
        s_network_tone.outage = NETWORK_OUTAGE_CLEAR;
    }

    network_tone_unlock();

    if (should_play) {
        network_tone_play(NETWORK_UNSTABLE_TONE_ID);
    }
}

static void network_tone_start_unstable_grace(const sys_network_status_t *status,
                                               bool status_ok)
{
    bool play_immediately = false;

    if (!network_status_is_unstable(status, status_ok)) {
        network_tone_cancel_unstable_grace();
        return;
    }

    if (!network_tone_lock()) {
        return;
    }

    /* 只在首次发现异常时启动，重复失败消息不会不断延长宽限期。 */
    if (s_network_tone.outage == NETWORK_OUTAGE_CLEAR) {
        s_network_tone.outage = NETWORK_OUTAGE_GRACE;
        if (s_network_tone.network_unstable_grace_timer == NULL ||
            xTimerReset(s_network_tone.network_unstable_grace_timer, 0) != pdPASS) {
            LOGW("failed to start network unstable grace timer, play immediately");
            s_network_tone.outage = NETWORK_OUTAGE_ANNOUNCED;
            play_immediately = true;
        }
    }

    network_tone_unlock();

    if (play_immediately) {
        network_tone_play(NETWORK_UNSTABLE_TONE_ID);
    }
}

static void network_tone_schedule_cloud_success(bool force_schedule)
{
    bool play_immediately = false;

    if (!network_tone_lock()) {
        return;
    }

    network_tone_clear_outage_locked();

    /*
     * 普通 CLOUD_CONNECTED 只有在此前确实断线时才提示恢复；
     * CLOUD_AUTH_SUCCESS 使用 force_schedule，允许首次鉴权成功也进行提示。
     */
    if (!force_schedule &&
        s_network_tone.recovery != CLOUD_RECOVERY_RECONNECT_PENDING) {
        network_tone_unlock();
        return;
    }

    if (s_network_tone.recovery == CLOUD_RECOVERY_SUCCESS_DELAYED ||
        s_network_tone.recovery == CLOUD_RECOVERY_SUCCESS_ANNOUNCED) {
        network_tone_unlock();
        return;
    }

    /* 延迟期间 Pushup TTS 或绑定消息可以取消该提示，避免音频相互抢占。 */
    if (s_network_tone.cloud_success_delay_timer != NULL &&
        xTimerReset(s_network_tone.cloud_success_delay_timer, 0) == pdPASS) {
        s_network_tone.recovery = CLOUD_RECOVERY_SUCCESS_DELAYED;
    } else {
        LOGW("failed to start cloud success delay timer, play immediately");
        s_network_tone.recovery = CLOUD_RECOVERY_SUCCESS_ANNOUNCED;
        play_immediately = true;
    }

    network_tone_unlock();

    if (play_immediately) {
        network_tone_play_success();
    }
}

/* ==================== EBUS 消息处理 ==================== */

/* 云连接、鉴权及 Pushup TTS 相关消息统一在此处转换为状态机动作。 */
static void network_tone_on_cloud_message(void *unused, uint32_t msg_id,
                                          void *data, uint32_t len,
                                          void *user_data)
{
    (void)unused;
    (void)user_data;

    switch (msg_id) {
    case VOICE_MSG_CLOUD_PUSHUP_TTS_URL:
        /* 云端主动 TTS 优先级更高，避免延迟成功提示与其连续或重叠播放。 */
        network_tone_cancel_success_delay();
        if (cloud_unbound_active()) {
            /* TTS 模块负责新旧 TTS 切换；这里只清理本地提示音。 */
            voice_player_tone_stop();
        }
        break;

    case VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS: {
        struct app_datas *app_data = get_app_datas();

        if (app_data != NULL) {
            app_data->auth_failed = 0;
        }
        /* 鉴权成功是明确结果，无需依赖此前是否记录过云断线。 */
        network_tone_schedule_cloud_success(true);
        break;
    }

    case VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED: {
        struct app_datas *app_data = get_app_datas();

        /* 失败结果会使此前候选的成功提示失效。 */
        network_tone_cancel_success_delay();
        if (app_data != NULL) {
            app_data->auth_failed = 1;
        }
        network_tone_play(CLOUD_AUTH_FAILED_TONE_ID);
        break;
    }

    case VOICE_MSG_CLOUD_OPEN_INFO: {
        uint32_t status = 0U;

        if (data == NULL || len != sizeof(status)) {
            return;
        }
        memcpy(&status, data, sizeof(status));
        if (status != CLOUD_OPEN_INFO_STATUS_BIND) {
            return;
        }

        /* 进入绑定引导时不再播放通用连接成功提示，避免连续播音。 */
        network_tone_cancel_success_delay();
        if (cloud_unbound_active()) {
            /* 未绑定状态由随后的云端 TTS 引导，不额外播放退出配网提示。 */
            voice_player_tone_stop();
            voice_player_tts_stop();
            return;
        }
        network_tone_play_bind();
        break;
    }

    case VOICE_MSG_CLOUD_CONNECTED: {
        bool suppress;

        if (!network_tone_lock()) {
            return;
        }
        suppress = s_network_tone.suppress_next_success;
        s_network_tone.suppress_next_success = false;
        network_tone_unlock();

        if (suppress) {
            /* BLE 配网流程已经有独立成功提示，不再重复播报云连接成功。 */
            network_tone_reset();
        } else {
            /* 普通连接事件只有在此前记录过断线时才会安排成功提示。 */
            network_tone_schedule_cloud_success(false);
        }
        break;
    }

    case VOICE_MSG_CLOUD_DISCONNECTED: {
        sys_network_status_t status = {0};
        bool status_ok = sys_network_get_status(&status) == 0;

        /* 再次断线会使尚未播放的成功提示失效。 */
        network_tone_cancel_success_delay();
        if (network_tone_skip_disconnect(&status, status_ok)) {
            return;
        }

        /* 记录重连前提，后续 CLOUD_CONNECTED 才能播报“连接成功”。 */
        if (network_tone_lock()) {
            s_network_tone.recovery = CLOUD_RECOVERY_RECONNECT_PENDING;
            network_tone_unlock();
        }
        network_tone_start_unstable_grace(&status, status_ok);
        break;
    }

    default:
        break;
    }
}

/* 物理链路与网络探测消息统一维护断网提示状态。 */
static void network_tone_on_link_message(void *unused, uint32_t msg_id,
                                         void *data, uint32_t len,
                                         void *user_data)
{
    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    switch (msg_id) {
    case VOICE_MSG_SYSTEM_NETWORK_CONNECTED:
        /* 链路恢复不等于互联网可用，只取消宽限期，等待探测结果。 */
        network_tone_cancel_unstable_grace();
        break;

    case VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL: {
        sys_network_status_t status = {0};
        bool status_ok = sys_network_get_status(&status) == 0;

        /* 探测失败可能只是瞬时抖动，先进入宽限期而不立即播报。 */
        if (!network_tone_skip_disconnect(&status, status_ok)) {
            network_tone_start_unstable_grace(&status, status_ok);
        }
        break;
    }

    case VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS: {
        bool outage_announced = false;

        if (!network_tone_lock()) {
            return;
        }
        network_tone_cancel_unstable_grace_locked();
        /* 只有已经提示过异常，恢复时才补充成功提示，避免无故播报。 */
        if (s_network_tone.outage == NETWORK_OUTAGE_ANNOUNCED) {
            s_network_tone.outage = NETWORK_OUTAGE_CLEAR;
            outage_announced = true;
        }
        network_tone_unlock();

        if (outage_announced) {
            network_tone_play_success();
        }
        break;
    }

    case VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED: {
        sys_network_status_t status = {0};
        bool status_ok = sys_network_get_status(&status) == 0;
        bool should_play = false;

        /* 物理链路明确断开，不再等待“不稳定”宽限期。 */
        network_tone_cancel_success_delay();
        network_tone_cancel_unstable_grace();
        if (network_tone_skip_disconnect(&status, status_ok)) {
            return;
        }

        if (!network_tone_lock()) {
            return;
        }
        /* ANNOUNCED 同时承担去重标记，连续断线事件只播报一次。 */
        if (s_network_tone.outage != NETWORK_OUTAGE_ANNOUNCED) {
            s_network_tone.outage = NETWORK_OUTAGE_ANNOUNCED;
            should_play = true;
        }
        network_tone_unlock();

        if (should_play) {
            network_tone_play(NETWORK_DISCONNECTED_TONE_ID);
        }
        break;
    }

    default:
        break;
    }
}

/* BLE 鉴权结果只影响下一次云成功提示，连接完成播放配网成功提示。 */
static void network_tone_on_ble_message(void *unused, uint32_t msg_id,
                                        void *data, uint32_t len,
                                        void *user_data)
{
    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    switch (msg_id) {
    case VOICE_MSG_BLE_AUTH_INFO_DONE:
        if (!network_tone_lock()) {
            return;
        }
        /* 配网后的下一次云连接由 BLE 流程负责反馈，避免两次成功提示。 */
        s_network_tone.suppress_next_success =
            voice_cloud_get_state() == VOICE_CLOUD_STATE_CONNECT_FAILED;
        network_tone_unlock();
        break;

    case VOICE_MSG_BLE_CONNECT_DONE:
        network_tone_play(NETCFG_SUCCESS_TONE_ID);
        break;

    default:
        break;
    }
}

/* ==================== 内部工具：资源初始化 ==================== */

static int network_tone_create_runtime_resources(void)
{
    /* 各句柄只创建一次，避免重复分配 RTOS 资源。 */
    if (s_network_tone.lock == NULL) {
        s_network_tone.lock = xSemaphoreCreateMutex();
        if (s_network_tone.lock == NULL) {
            LOGE("failed to create network tone state lock");
            return -1;
        }
    }

    /* 两个定时器均为单次定时器，xTimerReset 用于启动或重新计时。 */
    if (s_network_tone.cloud_success_delay_timer == NULL) {
        s_network_tone.cloud_success_delay_timer =
            xTimerCreate("cloud.success.delay",
                         pdMS_TO_TICKS(CLOUD_SUCCESS_TONE_DELAY_MS),
                         pdFALSE,
                         NULL,
                         network_tone_cloud_success_delay_timer_cb);
        if (s_network_tone.cloud_success_delay_timer == NULL) {
            LOGE("failed to create cloud success delay timer");
            return -1;
        }
    }

    if (s_network_tone.network_unstable_grace_timer == NULL) {
        s_network_tone.network_unstable_grace_timer =
            xTimerCreate("net.unstable.grace",
                         pdMS_TO_TICKS(NETWORK_UNSTABLE_TONE_GRACE_MS),
                         pdFALSE,
                         NULL,
                         network_tone_unstable_grace_timer_cb);
        if (s_network_tone.network_unstable_grace_timer == NULL) {
            LOGE("failed to create network unstable grace timer");
            return -1;
        }
    }

    return 0;
}

/* ==================== 对外 API ==================== */

int voice_player_network_tone_init(void)
{
    sys_network_status_t status = {0};
    bool wifi_mode;
    int ret = 0;

    if (network_tone_create_runtime_resources() != 0) {
        return -1;
    }

    network_tone_reset();

    /* 云连接、鉴权及云端业务消息。 */
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, network_tone_on_cloud_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS, network_tone_on_cloud_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, network_tone_on_cloud_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, network_tone_on_cloud_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_OPEN_INFO, network_tone_on_cloud_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, network_tone_on_cloud_message, NULL);

    /* 物理链路状态及互联网可用性探测消息。 */
    ret |= voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_CONNECTED, network_tone_on_link_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED, network_tone_on_link_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, network_tone_on_link_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, network_tone_on_link_message, NULL);

    /* BLE 配网流程消息。 */
    ret |= voice_msg_sub(VOICE_MSG_BLE_AUTH_INFO_DONE, network_tone_on_ble_message, NULL);
    ret |= voice_msg_sub(VOICE_MSG_BLE_CONNECT_DONE, network_tone_on_ble_message, NULL);

    /* Wi-Fi 模式下没有已保存 AP，启动后直接提示用户进入配网。 */
    wifi_mode = sys_network_get_status(&status) == 0 &&
                status.mode == SYS_NETWORK_MODE_WIFI;
    if (wifi_mode && !sys_wifi_has_ap()) {
        network_tone_play(NETCFG_ENTER_TONE_ID);
    }

    return ret == 0 ? 0 : -1;
}
