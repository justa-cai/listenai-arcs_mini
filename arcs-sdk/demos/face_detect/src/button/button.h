#pragma once


// #include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
// #define __containerof(ptr, type, member) \
//     ((type *)((char *)(ptr) - offsetof(type, member)))


#define LISA_RETURN_ON_FALSE(cond, ret)                     \
do {                                                        \
    if (!(cond)) {                                          \
        printf("[%s] %d: %d\n", __FILE__, __LINE__, ret);   \
        return ret;                                         \
    }                                                       \
} while (0)

#define LISA_GOTO_ON_FALSE(cond, err)                       \
do {                                                        \
    if (!(cond)) {                                          \
        printf("[%s] %d\n", __FILE__, __LINE__);            \
        goto err;                                           \
    }                                                       \
} while (0)

#define ENUM_TO_STR(e) (#e)

typedef struct {
    uint32_t short_press_time_ms;
    uint32_t long_press_time_ms;
    uint32_t long_hold_time_ms;
    uint32_t press_logic_level;
} button_config_t;

typedef enum {
    LISA_BTN_PRESS_DOWN = 0,
    LISA_BTN_PRESS_CLICK,
    LISA_BTN_PRESS_DOUBLE_CLICK,
    LISA_BTN_PRESS_REPEAT_CLICK,
    LISA_BTN_PRESS_SHORT_START,
    LISA_BTN_PRESS_SHORT_UP,
    LISA_BTN_PRESS_LONG_START,
    LISA_BTN_PRESS_LONG_UP,
    LISA_BTN_PRESS_LONG_HOLD,
    LISA_BTN_PRESS_LONG_HOLD_UP,
    LISA_BTN_PRESS_MAX,
    LISA_BTN_PRESS_NONE,
} button_event_t;

typedef void (*button_cb_t)(button_event_t evt, void *user);
