#include "FreeRTOS.h"
#include "task.h"

#define TAG "button.camera_preview"

#include "button_camera_preview.h"
#include "lisa_log.h"
#include "lisa_ui_nav_scr.h"
#include "voice_intent_mgr.h"
#include "voice_intent_photo_flow.h"
#include "voice_msg.h"

#define BUTTON_CAMERA_PREVIEW_GUARD_MS 3000U
#define BUTTON_CAMERA_PREVIEW_EXIT_POLL_MS 20U
#define BUTTON_CAMERA_PREVIEW_EXIT_TIMEOUT_MS 1500U

static voice_msg_camera_preview_state_t s_preview_state = {0};
static TickType_t s_button_enable_tick = 0;

static void button_camera_preview_state_changed(void *unused, uint32_t msg_id, void *data,
                                                uint32_t len, void *user_data);

static bool button_camera_preview_trigger_ready(void)
{
    return xTaskGetTickCount() >= s_button_enable_tick;
}

static bool camera_preview_is_mcp_locked(void)
{
    return s_preview_state.mode == CAMERA_PREVIEW_MODE_MCP &&
           (s_preview_state.phase == CAMERA_FLOW_PHASE_PREVIEW ||
            s_preview_state.phase == CAMERA_FLOW_PHASE_PROCESSING);
}

static bool camera_preview_is_button_active(void)
{
    return s_preview_state.mode == CAMERA_PREVIEW_MODE_BUTTON && s_preview_state.active;
}

static void button_camera_preview_publish_exit(void)
{
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
}

static void button_camera_preview_publish_capture(void)
{
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_CAPTURE, NULL, 0);
}

bool button_camera_preview_is_busy(void)
{
    return s_preview_state.phase != CAMERA_FLOW_PHASE_NONE;
}

bool button_camera_preview_request_exit(void)
{
    if (!button_camera_preview_is_busy()) {
        return false;
    }

    button_camera_preview_publish_exit();
    return true;
}

bool button_camera_preview_wait_exit(void)
{
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(BUTTON_CAMERA_PREVIEW_EXIT_TIMEOUT_MS);

    if (!button_camera_preview_request_exit()) {
        return true;
    }

    while (button_camera_preview_is_busy()) {
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_CAMERA_PREVIEW_EXIT_POLL_MS));
    }

    return true;
}

static void button_camera_preview_publish_start(void)
{
    voice_msg_camera_preview_req_t req = {0};

    /* 按键拍照是独立交互，进入预览前强制结束本地和云端语音会话，
     * 避免未结束的云端音频会话影响后续图片识别。 */
    if (voice_intent_contains(INTENT_VOICE_SESSION)) {
        LOGI("interrupt active voice session before button photo preview");
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, NULL, 0);
    }

    if (lisa_ui_nav_scr_get_top_id() != 0) {
        lisa_ui_nav_scr_nav_to(0);
    }

    req.mode = CAMERA_PREVIEW_MODE_BUTTON;
    req.auto_capture_delay_ms = 0;
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_START, &req, sizeof(req));
}

void button_camera_preview_init(void)
{
    s_button_enable_tick = xTaskGetTickCount() + pdMS_TO_TICKS(BUTTON_CAMERA_PREVIEW_GUARD_MS);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, button_camera_preview_state_changed, NULL);
}

static void button_camera_preview_state_changed(void *unused, uint32_t msg_id, void *data,
                                                uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!data || len < sizeof(s_preview_state)) {
        return;
    }
    s_preview_state = *(const voice_msg_camera_preview_state_t *)data;

    LOGI("camera preview state changed, active=%u mode=%u captured=%u phase=%u",
         s_preview_state.active,
         s_preview_state.mode,
         s_preview_state.captured,
         s_preview_state.phase);
}

bool button_camera_preview_handle_click(void)
{
    if (camera_preview_is_mcp_locked()) {
        LOGI("voice photo locked, cancel by button click, phase=%u", s_preview_state.phase);
        button_camera_preview_request_exit();
        return true;
    }

    if (camera_preview_is_button_active()) {
        LOGI("camera preview click, capture photo");
        button_camera_preview_publish_capture();
        return true;
    }

    return false;
}

void button_camera_preview_handle_double_click(void)
{
    if (camera_preview_is_mcp_locked()) {
        LOGI("voice photo locked, cancel by button double click, phase=%u", s_preview_state.phase);
        button_camera_preview_request_exit();
        return;
    }

    if (!button_camera_preview_trigger_ready()) {
        LOGI("ignore startup double click before photo preview trigger is armed");
        return;
    }

    LOGI("power button double click, enter photo preview");
    button_camera_preview_publish_start();
}

bool button_camera_preview_handle_long_hold(void)
{
    if (camera_preview_is_mcp_locked()) {
        LOGI("voice photo locked, cancel by button long hold, phase=%u", s_preview_state.phase);
        button_camera_preview_request_exit();
        return true;
    }

    if (camera_preview_is_button_active()) {
        LOGI("camera preview long hold, exit preview");
        button_camera_preview_request_exit();
        return true;
    }

    return false;
}
