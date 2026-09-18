#define TAG "app.button"

#include "alarm_handler.h"
#include "alarm_ring.h"
#include "apps/llm/lisa_ui_nav_scr_ids.h"
#include "apps/llm/models/model_qrcode.h"
#include "apps/llm/models/model_voice.h"
#include "lisa_log.h"
#include "lisa_ui_nav_scr.h"
#include "power/power_manager.h"
#include "tone.h"
#include "uboot_features_api.h"
#include "voice_cloud.h"
#include "miniapp.h"
#include "voice_intent_mgr.h"
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "voice_player/voice_player_tts.h"
#include "sys_wifi.h"

#ifdef CONFIG_OTA
#include "ota_manager.h"
#endif

#include "app_button.h"
#include "button_blecfg.h"
#include "button_camera_preview.h"
#include "button_factory_reset.h"
#include "service_power_policy.h"

/* ==================== 模块配置与状态 ==================== */

#define APP_BUTTON_PRIMARY_ID 0U

/* ==================== 版型相关展示策略 ==================== */

static bool app_button_cloud_info_page_enabled(void)
{
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    return false;
#else
    return true;
#endif
}

static void app_button_show_wifi_emoji(void)
{
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    static const char wifi_emoji[] = "wifi";

    voice_msg_pub(VOICE_MSG_CLOUD_EMOJI, (void *)wifi_emoji, sizeof(wifi_emoji));
#endif
}

/* 按键主动进入 Info 的策略独立于 main 的被动网络处理，便于两类场景后续分别演进。 */
static bool app_button_resolve_cloud_info_status(uint32_t *status_out)
{
    uint32_t status = QR_STATUS_CONNECTED;

    switch (voice_cloud_get_state()) {
    case VOICE_CLOUD_STATE_CONNECTING:
        return false;
    case VOICE_CLOUD_STATE_NO_NETWORK:
    case VOICE_CLOUD_STATE_NO_INTERNET:
        status = QR_STATUS_NOT_CONNECTED;
        break;
    case VOICE_CLOUD_STATE_TOKEN_FAILED:
        status = QR_STATUS_AUTH_FAILED;
        break;
    case VOICE_CLOUD_STATE_CONNECT_FAILED:
        status = voice_cloud_is_device_unbound() ? QR_STATUS_BIND : QR_STATUS_NOT_CONNECTED;
        break;
    default:
        return false;
    }

    if (status_out) {
        *status_out = status;
    }
    return true;
}

static void app_button_open_cloud_info(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    if (status == QR_STATUS_NOT_CONNECTED) {
        app_button_show_wifi_emoji();
        return;
    }
#endif
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, &status, sizeof(status));
#else
    (void)status;
#endif
}

/* 最终点击次数确定后再播报，避免多击过程中每次 SHORT_UP 都触发提示音。 */
static void app_button_prompt_cloud_info(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
    switch (status) {
    case QR_STATUS_NOT_CONNECTED:
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_70));
        break;
    case QR_STATUS_AUTH_FAILED:
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_105));
        break;
    default:
        break;
    }

    app_button_open_cloud_info(status);
#else
    (void)status;
#endif
}

/* ==================== 全局按键拦截策略 ==================== */

/*
 * OTA 确认页独占按键输入：单击确认升级，长按暂缓升级，其他动作忽略。
 * 返回 true 表示当前动作已被 OTA 流程消费，不应继续执行普通按键行为。
 */
static bool app_button_intercept_ota_confirmation_action(voice_msg_button_action_t action)
{
#ifdef CONFIG_OTA
    if (ota_manager_get_state() != OTA_STATE_PACKAGE_INFO) {
        return false;
    }

    if (!ota_manager_app_update_input_ready()) {
        LISA_LOGI(TAG, "Ignore button action=%d during OTA package info guard window", action);
        return true;
    }

    if (action == VOICE_MSG_BUTTON_ACTION_CLICK) {
        if (ota_manager_confirm_app_update() == 0) {
            LISA_LOGI(TAG, "User confirmed app OTA update");
        } else {
            LISA_LOGW(TAG, "Failed to confirm app OTA update");
        }
        return true;
    }

    if (action == VOICE_MSG_BUTTON_ACTION_LONG_HOLD) {
        if (ota_manager_skip_app_update() == 0) {
            LISA_LOGI(TAG, "User deferred app OTA update");
        } else {
            LISA_LOGW(TAG, "Failed to defer app OTA update");
        }
        return true;
    }

    LISA_LOGI(TAG, "Ignore button action=%d while waiting app OTA confirmation", action);
    return true;
#else
    (void)action;
    return false;
#endif
}

/*
 * 闹钟响铃期间独占按键输入：任意短按序列均视为一次稍后提醒，长按关闭闹钟，
 * 其他动作由闹钟流程消费，避免跳转 Info、拍照、配网或触发恢复出厂。
 */
static bool app_button_intercept_alarm_action(voice_msg_button_action_t action)
{
    if (!alarm_ring_is_active()) {
        return false;
    }

    switch (action) {
    case VOICE_MSG_BUTTON_ACTION_CLICK:
    case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_REPEAT_CLICK:
        LISA_LOGI(TAG, "Short click sequence: alarm ringing, handle snooze, action=%d", action);
        alarm_handle_snooze();
        alarm_ring_stop();
        return true;
    case VOICE_MSG_BUTTON_ACTION_LONG_HOLD:
        LISA_LOGI(TAG, "Long hold: alarm ringing, stop and generate next");
        alarm_ring_stop();
        alarm_handle_stop_and_next();
        return true;
    default:
        LISA_LOGI(TAG, "Ignore button action=%d while alarm is ringing", action);
        return true;
    }
}

/* 判断当前按键动作是否允许被“云端不可用”信息页拦截。 */
static bool app_button_cloud_info_can_redirect_action(voice_msg_button_action_t action)
{
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    if (action == VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK ||
        action == VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK ||
        action == VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK) {
        return false;
    }
#endif

    switch (action) {
    case VOICE_MSG_BUTTON_ACTION_CLICK:
    case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
        return true;
    default:
        return false;
    }
}

/*
 * 云端不可用且当前按键允许重定向时，打开对应的信息页。
 * 返回 true 表示已经完成页面跳转，原按键动作不应继续执行。
 */
static bool app_button_redirect_to_cloud_info(voice_msg_button_action_t action)
{
    uint32_t status = QR_STATUS_CONNECTED;

    if (!app_button_cloud_info_can_redirect_action(action) ||
        !app_button_resolve_cloud_info_status(&status)) {
        return false;
    }

    LISA_LOGI(TAG, "Button redirects to cloud info, status=%u", (unsigned)status);
    app_button_prompt_cloud_info(status);
    return true;
}

/* ==================== 按键动作 ==================== */

/*
 * 处理电源键单击动作，并按当前交互场景依次分流：
 * 拍照流程 > 页面导航 > 云端会话。
 * 闹钟响铃由全局拦截策略优先处理，不会进入此函数。
 */
static void app_button_handle_click(void)
{
    if (service_power_policy_handle_function_click()) {
        LISA_LOGI(TAG, "Single click: restored from hibernate without starting interaction");
        return;
    }

    /* 拍照流程优先：预览中执行拍照，语音拍照锁定时退出拍照流程。 */
    if (button_camera_preview_handle_click()) {
        return;
    }

    /* 当前不在主页时，单击只返回主页，不触发唤醒。 */
    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        LISA_LOGI(TAG, "Single click: not on home page, navigating home");
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
        return;
    }

    /* TTS 阶段单击需要打断回复并重新收音；其余会话阶段单击用于结束当前唤醒。 */
    if (model_voice_cloud_is_running() && !voice_player_tts_is_active()) {
        LISA_LOGI(TAG, "Single click: stop active wakeup");
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, NULL, 0);
    } else {
        const char wakeup_keyword[] = "xiao ling xiao ling";

#if CONFIG_WIFI
        /*
         * Standby deliberately uses the maximum listen interval. Exit Wi-Fi
         * power save before publishing the local wake so the outbound request
         * never waits for the next beacon.
         */
        int ret = sys_wifi_set_standby_power_save(false);
        if (ret != 0) {
            LISA_LOGW(TAG, "WiFi standby resume before button wake failed: %d", ret);
        }
#endif
        LISA_LOGI(TAG, "Single click: wakeup trigger");
        voice_msg_pub(VOICE_MSG_WAKEUP_KEYWORD,
                      (void *)wakeup_keyword,
                      sizeof(wakeup_keyword));
    }
}

/* 双击：进入按键拍照预览；若语音拍照流程已锁定，则取消并退出该流程。 */
static void app_button_handle_double_click(void)
{
    button_camera_preview_handle_double_click();
}

/* 三击信息页属于独立交互：先同步结束正在进行的语音会话，避免提示音结束后
 * 恢复到残留的 VOICE_SESSION。云端退出消息负责停止录音，TTS 立即停止，
 * 再从意图栈中移除会话，使后续 PROMPT_TONE 结束后直接恢复 MUSIC。 */
static void app_button_exit_voice_session_for_info(void)
{
    if (!voice_intent_contains(INTENT_VOICE_SESSION)) {
        return;
    }

    LISA_LOGI(TAG, "Triple click: exit active voice session before info page");
    /* 通知云端停止当前 chat，关闭录音及后续会话处理。 */
    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);

    /* 提示音不能与对话 TTS 重叠播放，立即清空当前 TTS 请求和播放器状态。 */
    if (voice_player_tts_is_active()) {
        voice_player_tts_stop();
    }

    /* 退出消息经异步总线处理；这里先同步收口本地意图栈，确保紧随其后的
     * PROMPT_TONE 出栈时直接恢复 MUSIC，而不会恢复已结束的语音会话。 */
    voice_intent_pop(INTENT_VOICE_SESSION);
}

/* 三击：根据当前云端状态播报提示音，并打开对应的信息或二维码页。 */
static void app_button_handle_triple_click(void)
{
    uint32_t status = QR_STATUS_CONNECTED;

    /* 将云端异常状态转换为未联网、未绑定等页面状态；云端正常时保持已连接状态。 */
    (void)app_button_resolve_cloud_info_status(&status);
    LISA_LOGI(TAG, "power button triple click, open info page");

    /* 三击可能发生在按键拍照预览期间。先发送统一的预览退出事件，
     * 让 PHOTO_FLOW 出栈并触发 MUSIC 恢复，再切换到二维码页面。 */
    if (button_camera_preview_is_busy()) {
        LISA_LOGI(TAG, "Triple click: exit active photo preview before info page");
        button_camera_preview_request_exit();
    }

    /* 三击配置页会中断正在进行的对话，确保提示音结束后恢复音乐而非旧会话。 */
    app_button_exit_voice_session_for_info();

    /* 已连接和未连接使用不同的信息页提示音。 */
    if (voice_cloud_is_connected()) {
        voice_player_play_prompt_tone_url(app_tone_get_url(TONE_ID_104));
    } else {
        voice_player_play_prompt_tone_url(app_tone_get_url(TONE_ID_64));
    }

    /* 部分版型不提供独立信息页，只保留提示音。 */
    if (!app_button_cloud_info_page_enabled()) {
        LISA_LOGI(TAG, "power button triple click, info page disabled on this board");
        return;
    }

    app_button_open_cloud_info(status);
}

/* 五至七击：交由 BLE 配网模块完成网络切换、清除 Wi-Fi 和配网页展示。 */
static void app_button_handle_netcfg_click(void)
{
    (void)button_blecfg_start();
}

/* 连击八次及以上：执行恢复出厂设置流程。 */
static void app_button_handle_repeat_click(void)
{
    LISA_LOGI(TAG, "power button repeat click, factory reset");
    button_factory_reset();
}

/*
 * 处理电源键长按动作，并按当前交互场景依次分流：
 * 闹钟响铃时长按关闭闹钟，由全局拦截策略优先处理；
 * 其他场景按拍照流程 > 设备关机分流。
 */
static void app_button_handle_long_hold(void)
{
    /* 拍照预览或语音拍照进行中时，长按只退出拍照流程。 */
    if (button_camera_preview_handle_long_hold()) {
        return;
    }

    /* 新 boot 下 power_shutdown 会按供电方式分流。老 boot 下 USB 供电时
     * 拉低 PWR_LOCK 无效，因此忽略关机，避免产生已经关机的错觉。 */
    if (!uboot_features_has(UBOOT_FEATURE_POWER_GUARD) && power_is_usb_plugged()) {
        LISA_LOGI(TAG, "USB connected on legacy boot, ignore long press shutdown");
        return;
    }

    /* 其他场景下，长按执行设备关机。 */
    LISA_LOGI(TAG, "power button long press hold, shutting down...");
    power_shutdown();
}

/* ==================== 动作分发与消息回调 ==================== */

static void app_button_dispatch_action(voice_msg_button_action_t action)
{
    switch (action) {
    case VOICE_MSG_BUTTON_ACTION_CLICK:
        /* 单击：预览页拍照或退出语音拍照；非主页返回主页；主页唤醒或结束会话。 */
        app_button_handle_click();
        break;
    case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
        /* 双击：进入拍照预览；语音拍照锁定时退出拍照流程。 */
        app_button_handle_double_click();
        break;
    case VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK:
        /* 三击：播报设备信息提示音并打开信息/二维码页。 */
        app_button_handle_triple_click();
        break;
    case VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK:
        /* 四击：预留，当前无操作。 */
        break;
    case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
        /* 五至七击：用户主动进入 BLE 配网。 */
        app_button_handle_netcfg_click();
        break;
    case VOICE_MSG_BUTTON_ACTION_REPEAT_CLICK:
        /* 连击八次及以上：恢复出厂设置。 */
        app_button_handle_repeat_click();
        break;
    case VOICE_MSG_BUTTON_ACTION_LONG_HOLD:
        /* 长按：响铃时由前置拦截关闭闹钟；否则退出拍照预览或执行关机。 */
        app_button_handle_long_hold();
        break;
    default:
        break;
    }
}

static void app_button_on_changed(void *unused, uint32_t msg_id, void *data,
                                  uint32_t len, void *user_data)
{
    const voice_msg_button_evt_t *evt = (const voice_msg_button_evt_t *)data;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (evt == NULL || len < sizeof(*evt) ||
        evt->button_id != APP_BUTTON_PRIMARY_ID) {
        return;
    }

    /* OTA 确认页优先级最高；返回 true 表示该动作已处理。 */
    if (app_button_intercept_ota_confirmation_action(evt->action)) {
        return;
    }

    /* 闹钟响铃期间独占输入：短按序列稍后提醒，长按关闭闹钟。 */
    if (app_button_intercept_alarm_action(evt->action)) {
        return;
    }

#ifdef CONFIG_MINIAPP
    if (miniapp_is_active()) {
        if (evt->action == VOICE_MSG_BUTTON_ACTION_CLICK) {
            (void)miniapp_button_click("function");
        }
        return;
    }
#endif

    /* 云端异常时，部分按键只负责打开错误或绑定信息页。 */
    if (app_button_redirect_to_cloud_info(evt->action)) {
        return;
    }

    app_button_dispatch_action(evt->action);
}

/* ==================== 对外接口 ==================== */

int app_button_init(void)
{
    int ret = 0;

    button_camera_preview_init();
    ret |= button_blecfg_init();

    ret |= voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, app_button_on_changed, NULL);

    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to initialize button module, ret=%d", ret);
    }
    return ret;
}
