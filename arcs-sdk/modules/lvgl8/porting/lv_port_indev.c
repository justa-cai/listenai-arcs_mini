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
#include "lisa_device.h"
#include "lisa_display.h"
#include <FreeRTOS.h>
#include <semphr.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
struct touch_msg {
    uint16_t x;
    uint16_t y;
    bool pressed;
};

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(lisa_device_t *touch_dev);
static void touchpad_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void touchpad_transform_coordinates(lv_coord_t *x, lv_coord_t *y);

/**********************
 *  STATIC VARIABLES
 **********************/
static lisa_device_t *touch_device = NULL;
static lv_indev_drv_t indev_drv;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static uint8_t last_state = 0;
static SemaphoreHandle_t touchpad_sem = NULL;
static QueueHandle_t touchpad_queue = NULL;

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
static void touch_int_callback(const lisa_touch_event_t *event, void *user_data)
{
    struct touch_msg msg;
    static bool last_pressed = false;

    if (event->type == LISA_TOUCH_EVENT_PRESS && event->point_count > 0) {
        msg.x = event->points[0].x;
        msg.y = event->points[0].y;
        msg.pressed = true;

        last_pressed = true;
    } else if (event->type == LISA_TOUCH_EVENT_RELEASE) {
        if (last_pressed) {
            msg.pressed = false;
            last_pressed = false;
        }
    }

    xQueueSend(touchpad_queue, &msg, portMAX_DELAY);
}

void lv_port_indev_init(lisa_device_t *touch_dev)
{
    /*Initialize your touchpad*/
    touchpad_init(touch_dev);
    if (touch_device == NULL) {
        return;
    }

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your touchpad*/
static void touchpad_init(lisa_device_t *touch_dev)
{
    touch_device = touch_dev;
    if (touch_device == NULL) {
        return;
    }

    int ret = lisa_touch_set_callback(touch_dev, touch_int_callback, NULL);
    if (ret != 0) {
        printf("Error: Touch callback set failed (code: %d)\n", ret);
        return;
    }

    ret = lisa_touch_set_int_mode(touch_dev, LISA_TOUCH_INT_MODE_INTERRUPT);
    if (ret != 0) {
        printf("Error: Touch interrupt mode set failed (code: %d)\n", ret);
        return;
    }

    ret = lisa_touch_enable(touch_dev);
    if (ret != 0) {
        printf("Error: Touch enable failed (code: %d)\n", ret);
        return;
    }

    touchpad_sem = xSemaphoreCreateBinary();
    touchpad_queue = xQueueCreate(CONFIG_LS_LV_INDEV_TASK_QUEUE_SIZE, sizeof(struct touch_msg));
}

/* Will be called by the library to read the touchpad */
static void touchpad_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    struct touch_msg msg;

    if (xQueueReceive(touchpad_queue, &msg, 0) == pdPASS) {
        data->point.x = (lv_coord_t)msg.x;
        data->point.y = (lv_coord_t)msg.y;
        data->state = msg.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        data->continue_reading = (uxQueueMessagesWaiting(touchpad_queue) > 0 ? 1 : 0);

        touchpad_transform_coordinates(&data->point.x, &data->point.y);

        last_x = data->point.x;
        last_y = data->point.y;
        last_state = data->state;
        return;
    }

    data->point.x = (lv_coord_t)last_x;
    data->point.y = (lv_coord_t)last_y;
    data->state = last_state;
    data->continue_reading = 0;
}

/**
 * @brief 触摸坐标系转换
 * @param x 输入/输出 X 坐标
 * @param y 输入/输出 Y 坐标
 *
 * @note 该函数处理硬件坐标系校正（触摸屏与显示屏坐标系不一致）
 */
static void touchpad_transform_coordinates(lv_coord_t *x, lv_coord_t *y)
{
    if (!x || !y) {
        return;
    }

    extern lisa_device_t *lv_display_device;
    lisa_display_capabilities_t caps = {0};
    lisa_display_get_capabilities(lv_display_device, &caps);

#if CONFIG_LV_POINTER_SWAP_XY
    lv_coord_t cur_x = *y;
    lv_coord_t cur_y = *x;
#else
    lv_coord_t cur_x = *x;
    lv_coord_t cur_y = *y;
#endif

#if CONFIG_LV_POINTER_INVERT_X
    cur_x = caps.x_resolution - cur_x;
#endif

#if CONFIG_LV_POINTER_INVERT_Y
    cur_y = caps.y_resolution - cur_y;
#endif

    *x = cur_x;
    *y = cur_y;
}

#else /* Enable this file at the top */

/* This dummy typedef exists purely to silence -Wpedantic. */
typedef int keep_pedantic_happy;
#endif
