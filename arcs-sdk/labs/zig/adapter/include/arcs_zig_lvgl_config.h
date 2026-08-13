#ifndef ARCS_ZIG_LVGL_CONFIG_H
#define ARCS_ZIG_LVGL_CONFIG_H

#if defined(__has_include)
#if __has_include("autoconf.h")
#include "autoconf.h"
#endif
#endif

#ifndef LV_CONF_SKIP
#define LV_CONF_SKIP 1
#endif

#include "lv_conf_internal.h"

enum {
    ARCS_ZIG_LVGL_COLOR_DEPTH = LV_COLOR_DEPTH,
    ARCS_ZIG_LVGL_BIG_ENDIAN_SYSTEM = LV_BIG_ENDIAN_SYSTEM,
    ARCS_ZIG_LVGL_USE_ASSERT_STYLE = LV_USE_ASSERT_STYLE,
    ARCS_ZIG_LVGL_USE_USER_DATA = LV_USE_USER_DATA,
    ARCS_ZIG_LVGL_USE_LARGE_COORD = LV_USE_LARGE_COORD,
};

#endif /* ARCS_ZIG_LVGL_CONFIG_H */
