#include "lisa_ui.h"
#include "lisa_ui_log.h"
#include "lisa_ui_nav_scr.h"

#include "model_camera.h"
#include "model_common.h"
#include "model_voice.h"
#include "model_wifi.h"
#include "model_qrcode.h"
#include "model_alarm.h"
#include "model_battery.h"

extern void alarm_navigation_init(void);
extern const struct lisa_ui_nav_scr home_nav_scr;
extern const struct lisa_ui_nav_scr info_nav_scr;
extern const struct lisa_ui_nav_scr setting_nav_scr;
extern const struct lisa_ui_nav_scr setting_common_nav_scr;
extern const struct lisa_ui_nav_scr setting_wifi_nav_scr;
extern const struct lisa_ui_nav_scr setting_wakeup_nav_scr;
extern const struct lisa_ui_nav_scr setting_clock_nav_scr;
extern const struct lisa_ui_nav_scr alarm_success_nav_scr;
extern const struct lisa_ui_nav_scr alarm_ring_nav_scr;
extern const struct lisa_ui_nav_scr quota_qrcode_nav_scr;
#ifdef CONFIG_OTA
extern const struct lisa_ui_nav_scr ota_nav_scr;
#endif

static int lisa_ui_model_init(void)
{
    model_wifi_init();
    model_voice_init();
    model_common_init();
    model_alarm_init();
    // model_camera_init();
    model_qrcode_init();
    model_battery_init();

    return 0;
}

int lisa_ui_app_init(void)
{
    lisa_ui_model_init();
    
    /* 初始化闹钟导航处理器 */
    alarm_navigation_init();

    lisa_ui_nav_scr_add(&home_nav_scr);
    lisa_ui_nav_scr_add(&info_nav_scr);
    lisa_ui_nav_scr_add(&setting_nav_scr);
    lisa_ui_nav_scr_add(&setting_common_nav_scr);
    lisa_ui_nav_scr_add(&setting_wifi_nav_scr);
    lisa_ui_nav_scr_add(&setting_wakeup_nav_scr);
    lisa_ui_nav_scr_add(&setting_clock_nav_scr);
    lisa_ui_nav_scr_add(&alarm_success_nav_scr);
    lisa_ui_nav_scr_add(&alarm_ring_nav_scr);
    lisa_ui_nav_scr_add(&quota_qrcode_nav_scr);
#ifdef CONFIG_OTA
    lisa_ui_nav_scr_add(&ota_nav_scr);
#endif

    /* fisrt src will be open and show now */
    lisa_ui_nav_scr_default_set(&home_nav_scr);

    return 0;
}
