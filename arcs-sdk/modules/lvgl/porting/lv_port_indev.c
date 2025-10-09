/**
 * @file lv_port_indev.c
 *
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lisa_touch.h"
#include "lisa_display.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(touch_hw_config_t *config);
static bool touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t *x, lv_coord_t *y, bool *pressed);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t *indev_touchpad;
static const struct touch_device *touch_device = NULL;
static volatile bool touch_pressed = false;
static uint16_t last_x = 0;
static uint16_t last_y = 0;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief Touch interrupt callback function
 *
 * This callback is called from interrupt context when touch event occurs.
 * It should be kept as short as possible.
 */
static void touch_int_callback(void)
{
    touch_pressed = true;
}

void lv_port_indev_init(touch_hw_config_t *config)
{
    /*Initialize your touchpad*/
    touchpad_init(config);
    if (touch_device == NULL) {
        return;
    }

    /*Register a touchpad input device*/
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your touchpad*/
static void touchpad_init(touch_hw_config_t *config)
{
    touch_device = lisa_touch_create(config);
    if (touch_device == NULL) {
        return;
    }

#ifdef CONFIG_LISA_TOUCH_INTERRUPT
    /* Register interrupt callback */
    lisa_touch_set_int_callback(touch_device, touch_int_callback);
#endif
}

/* Will be called by the library to read the touchpad */
static bool touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    bool pressed;

    /*Save the pressed coordinates and the state*/
    if (touchpad_is_pressed()) {
        touchpad_get_xy(&data->point.x, &data->point.y, &pressed);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;
    }

    LV_LOG_INFO("[%s] pressed: %d, x: %d, y: %d", __func__, data->state, data->point.x, data->point.y);

    return false; /*No buffering now so no more data read*/
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    bool pressed = touch_pressed;
    touch_pressed = false;
    return pressed;
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t *x, lv_coord_t *y, bool *pressed)
{
    lv_coord_t cur_x = 0, cur_y = 0;
    extern const struct display_device *lv_display_device;
    struct display_capabilities caps = {0};

    if (lisa_touch_read_coordinates(touch_device, (uint16_t *)&cur_x, (uint16_t *)&cur_y, pressed) == 0) {
        lisa_display_get_capabilities(lv_display_device, &caps);
        last_x = cur_x;
        last_y = cur_y;

        /*
            因屏幕硬件本身调整坐标系
            比如：touch本身坐标系和屏幕坐标系不一致
         */
#if CONFIG_LV_POINTER_SWAP_XY
        last_x = cur_y;
        last_y = cur_x;
#endif

#if CONFIG_LV_POINTER_INVERT_X
        last_x = caps.x_resolution - last_x;
#endif

#if CONFIG_LV_POINTER_INVERT_Y
        last_y = caps.y_resolution - last_y;
#endif

        /*
            因屏幕旋转调整坐标系
         */
        if (caps.current_orientation == DISPLAY_ORIENTATION_ROTATED_90) {
            lv_coord_t temp_x = last_y;
            lv_coord_t temp_y = caps.x_resolution - last_x;
            last_x = temp_x;
            last_y = temp_y;
        } else if (caps.current_orientation == DISPLAY_ORIENTATION_ROTATED_270) {
            lv_coord_t temp_x = caps.y_resolution - last_y;
            lv_coord_t temp_y = last_x;
            last_x = temp_x;
            last_y = temp_y;
        } else if (caps.current_orientation == DISPLAY_ORIENTATION_ROTATED_180) {
            lv_coord_t temp_x = caps.x_resolution - last_x;
            lv_coord_t temp_y = caps.y_resolution - last_y;
            last_x = temp_x;
            last_y = temp_y;
        }

        *x = last_x;
        *y = last_y;
    }
}

#else /* Enable this file at the top */

/* This dummy typedef exists purely to silence -Wpedantic. */
typedef int keep_pedantic_happy;
#endif
