#include "stdint.h"
#include "lisa_display.h"
#include "kv.h"
#include "lisa_kv.h"

#define APP_DISPLAY_BRIGHTNESS_DEFAULT 100

#define APP_DISPLAY_BRIGHTNESS_MIN 10
#define APP_DISPLAY_BRIGHTNESS_MAX 100

#define APP_DISPLAY_BRIGHTNESS_REVERT 1

int app_display_set_brightness(uint8_t val)
{
    if (val > APP_DISPLAY_BRIGHTNESS_MAX) {
        val = APP_DISPLAY_BRIGHTNESS_MAX;
    }

    if (val < APP_DISPLAY_BRIGHTNESS_MIN) {
        val = APP_DISPLAY_BRIGHTNESS_MIN;
    }

    uint8_t saved = val;
#if APP_DISPLAY_BRIGHTNESS_REVERT
    val = APP_DISPLAY_BRIGHTNESS_MAX - val;
#endif

    lisa_display_set_brightness(lisa_display_get(), val);
    lisa_kv_set_int(KV_KEY_USER_BRIGHTNESS, saved);

    return 0;
}

uint8_t app_display_get_brightness(void)
{
    int val = APP_DISPLAY_BRIGHTNESS_DEFAULT;

    int r = lisa_kv_get_int(KV_KEY_USER_BRIGHTNESS, &val);

    return val;
}

int app_display_brightness_init(void)
{
    uint8_t val = app_display_get_brightness();
    app_display_set_brightness(val);

    return 0;
}
