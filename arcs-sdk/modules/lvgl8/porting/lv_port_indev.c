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
#include <assert.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

static SemaphoreHandle_t touchpad_sem = NULL;
static QueueHandle_t touchpad_queue = NULL;

#if (CONFIG_LS_LV_INDEV_TASK_FILTERING)
#define USE_TOUCH_TASK_FILTERING   (1)
#else
#define USE_TOUCH_TASK_FILTERING   (0)
#endif

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
static void touchpad_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void touchpad_get_xy(lv_coord_t *x, lv_coord_t *y, bool *pressed);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t *indev_touchpad;
static lv_indev_drv_t indev_drv;
static const struct touch_device *touch_device = NULL;
static volatile bool touch_pressed = false;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static uint8_t last_state = 0;

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
#if USE_TOUCH_TASK_FILTERING
    BaseType_t yield = pdFALSE;
    xSemaphoreGiveFromISR(touchpad_sem, &yield);
    portYIELD_FROM_ISR(yield);
#else
    touch_pressed = true;
#endif
}

void lv_port_indev_init(touch_hw_config_t *config)
{
    /*Initialize your touchpad*/
    touchpad_init(config);
    if (touch_device == NULL) {
        return;
    }

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

struct touch_msg {
    uint16_t x;
    uint16_t y;
    bool pressed;
};

void touchpad_task(void *p)
{
    const struct touch_device *touch_device = p;
    struct touch_msg msg;

    assert(touchpad_queue);
    assert(touchpad_sem);

    while(1) {
        xSemaphoreTake(touchpad_sem, portMAX_DELAY);
        if (lisa_touch_read_coordinates(touch_device, &msg.x, &msg.y, &msg.pressed) == 0) {

#if CONFIG_LISA_TOUCH_READ_FREQUENCY > 0
            static uint32_t last_read_time_ms = 0;

            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_read_time_ms < (1000 / CONFIG_LISA_TOUCH_READ_FREQUENCY)) {
                if (msg.pressed) {
                    continue;
                }
            }
            last_read_time_ms = now;
#endif
            xQueueSend(touchpad_queue, &msg, portMAX_DELAY);
        }
    }
}

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

    touchpad_sem = xSemaphoreCreateBinary();
#if USE_TOUCH_TASK_FILTERING
    touchpad_queue = xQueueCreate(CONFIG_LS_LV_INDEV_TASK_QUEUE_SIZE, sizeof(struct touch_msg));
    xTaskCreate(touchpad_task, "touchpad_task", CONFIG_LS_LV_INDEV_TASK_STACK_SIZE, (void *)touch_device,
                CONFIG_LS_LV_INDEV_TASK_PRIO, NULL);
#endif
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    bool pressed = touch_pressed;
    touch_pressed = false;
    return pressed;
}

/* Will be called by the library to read the touchpad */
static void touchpad_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
#if USE_TOUCH_TASK_FILTERING
    struct touch_msg msg;
    if (xQueueReceive(touchpad_queue, &msg, 0) == pdPASS) {
        data->point.x = msg.x;
        data->point.y = msg.y;
        data->state = msg.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        data->continue_reading = uxQueueMessagesWaiting(touchpad_queue) > 0 ? true:false;

        last_x = data->point.x;
        last_y = data->point.y;
        last_state = data->state;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = last_state;
        data->continue_reading = false;
    }
#else
    bool pressed;
    if (touchpad_is_pressed()) {
        touchpad_get_xy(&data->point.x, &data->point.y, &pressed);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;
    }
#endif
    LV_LOG_INFO("[%s] pressed: %d, x: %d, y: %d", __func__, data->state, data->point.x, data->point.y);
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

        *x = last_x;
        *y = last_y;
    }
}

#else /* Enable this file at the top */

/* This dummy typedef exists purely to silence -Wpedantic. */
typedef int keep_pedantic_happy;
#endif
