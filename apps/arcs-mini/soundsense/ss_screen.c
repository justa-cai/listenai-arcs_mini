/*
 * ss_screen.c - auto blanking + event wake for the SoundSense build
 *
 * The panel blanks after inactivity: 30 s during the day, 10 s from
 * 22:00 to 07:00 (wall clock, default Asia/Shanghai timezone). Blanking
 * is display-only (lisa_display_blanking_on + brightness 0): the system,
 * audio tap and WebSocket stream keep running.
 *
 * Wake sources (each restarts the blank timer):
 *   - ss_screen_activity(): button click, voice session start
 *   - ss_screen_on_event(): snoring/baby_cry class_start/end
 */
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "IOMuxManager.h"
#include "lisa_display.h"
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "lisa_log.h"
#include "lisa_pwm.h"
#include "lisa_thread.h"
#include "listen_system.h"
#include "semphr.h"
#include "task.h"
#include "voice_msg.h"

#include "pinmux.h" /* LCD_PWM_PIN */

#include "ss_screen.h"

#define TAG "ss"

#define SS_BLANK_DAY_S 30u
#define SS_BLANK_NIGHT_S 10u
#define SS_NIGHT_START_H 22
#define SS_NIGHT_END_H 7
#define SS_TICK_MS 1000u

static struct {
    bool blanked;
    uint8_t saved_brightness; /* user brightness before blanking */
    TickType_t last_activity;
    SemaphoreHandle_t lock;
    lisa_device_t *disp;
} st;

static bool ss_is_night(void)
{
    struct tm cal;
    if (!ls_sys_time_is_valid() || ls_sys_get_localtime(&cal) != 0) {
        return false; /* fake clock: use the (longer) daytime timeout */
    }
    return cal.tm_hour >= SS_NIGHT_START_H || cal.tm_hour < SS_NIGHT_END_H;
}

static void ss_blank(bool on)
{
    if (!st.disp) {
        return;
    }
    if (on && !st.blanked) {
        /* Only turn off the backlight — do NOT call lisa_display_blanking_on():
         * it puts the panel to sleep and disrupts LVGL's refresh cycle,
         * causing a NULL style crash when the screen wakes on button press.
         * LVGL keeps rendering to the dark panel; we just kill the light. */
        lisa_device_t *pwm = lisa_device_get("pwm0");
        if (pwm) {
            lisa_pwm_disable(pwm, 1);
        }
        lisa_device_t *gpioa = lisa_device_get("gpioa");
        if (gpioa) {
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, 0); /* GPIO mode */
            lisa_gpio_configure(gpioa, LCD_PWM_PIN,
                                LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
        }
        st.blanked = true;
        LISA_LOGI(TAG, "screen: blanked (backlight off, LVGL running)");
    } else if (!on && st.blanked) {
        lisa_device_t *gpioa = lisa_device_get("gpioa");
        if (gpioa) {
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, 12); /* PWM mode */
        }
        lisa_device_t *pwm = lisa_device_get("pwm0");
        if (pwm) {
            lisa_pwm_enable(pwm, 1);
        }
        st.blanked = false;
        LISA_LOGI(TAG, "screen: awake (backlight on)");
    }
}

static void ss_task(void *arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SS_TICK_MS));

        if (!st.lock) {
            continue;
        }
        xSemaphoreTake(st.lock, portMAX_DELAY);
        uint32_t timeout_s = ss_is_night() ? SS_BLANK_NIGHT_S : SS_BLANK_DAY_S;
        uint32_t idle_s = (xTaskGetTickCount() - st.last_activity) / pdMS_TO_TICKS(1000);
        if (!st.blanked && idle_s >= timeout_s) {
            ss_blank(true);
        }
        xSemaphoreGive(st.lock);
    }
}

static void ss_wakeup_msg_cb(void *unused, uint32_t msg_id, void *data, uint32_t len,
                             void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;
    ss_screen_activity();
}

int ss_screen_init(void)
{
    if (st.lock) {
        return 0;
    }
    st.disp = lisa_device_get("display");
    if (!st.disp) {
        LISA_LOGW(TAG, "screen: no display device, blanking disabled");
        return -1;
    }
    st.lock = xSemaphoreCreateMutex();
    st.last_activity = xTaskGetTickCount();

    /* voice session start wakes the screen */
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, ss_wakeup_msg_cb, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, ss_wakeup_msg_cb, NULL);

    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"ss.screen",
        .stack_size = 3072,
        .priority = LISA_OS_PRIORITY_LOW,
    };
    if (!lisa_thread_create(&attr, ss_task, NULL)) {
        return -1;
    }
    LISA_LOGI(TAG, "screen: blanking manager started (%us/%us)",
              SS_BLANK_DAY_S, SS_BLANK_NIGHT_S);
    return 0;
}

void ss_screen_activity(void)
{
    if (!st.lock) {
        return;
    }
    xSemaphoreTake(st.lock, portMAX_DELAY);
    st.last_activity = xTaskGetTickCount();
    ss_blank(false);
    xSemaphoreGive(st.lock);
}

void ss_screen_on_event(const ss_event_t *ev)
{
    /* wake the screen (silent: no buzzer in the nursery) */
    ss_screen_activity();

    if (ev->is_start) {
        /* show the "waiting" face via the existing emoji channel */
        static const char alert_emoji[] = "等待";
        voice_msg_pub(VOICE_MSG_CLOUD_EMOJI, (void *)alert_emoji, sizeof(alert_emoji));
    }
}

/* manual blank control for the `ss blank <0|1>` debug command */
void ss_screen_test_blank(int on)
{
    if (!st.lock) {
        if (st.disp == NULL) {
            st.disp = lisa_device_get("display");
        }
        if (!st.disp) {
            return;
        }
        st.lock = xSemaphoreCreateMutex();
        st.last_activity = xTaskGetTickCount();
    }
    xSemaphoreTake(st.lock, portMAX_DELAY);
    ss_blank(on ? true : false);
    xSemaphoreGive(st.lock);
}
