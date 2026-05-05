/**
 * @file lv_port_indev_multi.c
 */

#include "lv_port_indev_multi.h"
#include "lisa_display.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <stddef.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} lv_port_touch_msg_t;

typedef struct {
    lisa_device_t *touch_dev;
    lisa_device_t *display_dev;
    lv_disp_t *target_disp;
    lv_indev_drv_t indev_drv;
    lv_indev_t *indev;
    QueueHandle_t queue;
    uint16_t last_x;
    uint16_t last_y;
    uint8_t last_state;
} lv_port_indev_multi_ctx_t;

static void touchpad_read_multi(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void touchpad_transform_coordinates_multi(lv_port_indev_multi_ctx_t *ctx, lv_coord_t *x, lv_coord_t *y);

static lv_port_indev_multi_ctx_t *lv_port_indev_ctx_from_drv(struct _lv_indev_drv_t *indev_drv)
{
    return (lv_port_indev_multi_ctx_t *)((char *)indev_drv - offsetof(lv_port_indev_multi_ctx_t, indev_drv));
}

static void touch_int_callback_multi(const lisa_touch_event_t *event, void *user_data)
{
    lv_port_indev_multi_ctx_t *ctx = (lv_port_indev_multi_ctx_t *)user_data;
    lv_port_touch_msg_t msg = {0};
    static bool last_pressed = false;

    if (!ctx || !ctx->queue) {
        return;
    }

    if (event->type == LISA_TOUCH_EVENT_PRESS && event->point_count > 0) {
        msg.x = event->points[0].x;
        msg.y = event->points[0].y;
        msg.pressed = true;
        last_pressed = true;
    } else if (event->type == LISA_TOUCH_EVENT_RELEASE) {
        if (!last_pressed) {
            return;
        }
        msg.pressed = false;
        last_pressed = false;
    } else {
        return;
    }

    xQueueSend(ctx->queue, &msg, portMAX_DELAY);
}

lv_indev_t *lv_port_indev_register(lisa_device_t *touch_dev, lisa_device_t *display_dev, lv_disp_t *target_disp)
{
    lv_port_indev_multi_ctx_t *ctx;
    int ret;

    if (!touch_dev || !display_dev || !target_disp) {
        return NULL;
    }

    ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) {
        LV_LOG_ERROR("[%s] context alloc failed", __FUNCTION__);
        return NULL;
    }
    lv_memset_00(ctx, sizeof(*ctx));

    ctx->touch_dev = touch_dev;
    ctx->display_dev = display_dev;
    ctx->target_disp = target_disp;
    ctx->queue = xQueueCreate(CONFIG_LS_LV_INDEV_TASK_QUEUE_SIZE, sizeof(lv_port_touch_msg_t));
    if (!ctx->queue) {
        LV_LOG_ERROR("[%s] queue create failed", __FUNCTION__);
        lv_mem_free(ctx);
        return NULL;
    }

    ret = lisa_touch_set_callback(touch_dev, touch_int_callback_multi, ctx);
    if (ret != 0) {
        LV_LOG_ERROR("[%s] touch callback set failed: %d", __FUNCTION__, ret);
        vQueueDelete(ctx->queue);
        lv_mem_free(ctx);
        return NULL;
    }

    ret = lisa_touch_set_int_mode(touch_dev, LISA_TOUCH_INT_MODE_INTERRUPT);
    if (ret != 0) {
        LV_LOG_ERROR("[%s] touch int mode set failed: %d", __FUNCTION__, ret);
        vQueueDelete(ctx->queue);
        lv_mem_free(ctx);
        return NULL;
    }

    ret = lisa_touch_enable(touch_dev);
    if (ret != 0) {
        LV_LOG_ERROR("[%s] touch enable failed: %d", __FUNCTION__, ret);
        vQueueDelete(ctx->queue);
        lv_mem_free(ctx);
        return NULL;
    }

    lv_indev_drv_init(&ctx->indev_drv);
    ctx->indev_drv.type = LV_INDEV_TYPE_POINTER;
    ctx->indev_drv.read_cb = touchpad_read_multi;
    ctx->indev_drv.disp = target_disp;
    ctx->indev = lv_indev_drv_register(&ctx->indev_drv);

    return ctx->indev;
}

static void touchpad_read_multi(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    lv_port_indev_multi_ctx_t *ctx = lv_port_indev_ctx_from_drv(indev_drv);
    lv_port_touch_msg_t msg;

    if (!ctx) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = 0;
        return;
    }

    if (xQueueReceive(ctx->queue, &msg, 0) == pdPASS) {
        data->point.x = (lv_coord_t)msg.x;
        data->point.y = (lv_coord_t)msg.y;
        data->state = msg.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = uxQueueMessagesWaiting(ctx->queue) > 0 ? 1 : 0;

        touchpad_transform_coordinates_multi(ctx, &data->point.x, &data->point.y);
        ctx->last_x = data->point.x;
        ctx->last_y = data->point.y;
        ctx->last_state = data->state;
        return;
    }

    data->point.x = (lv_coord_t)ctx->last_x;
    data->point.y = (lv_coord_t)ctx->last_y;
    data->state = ctx->last_state;
    data->continue_reading = 0;
}

static void touchpad_transform_coordinates_multi(lv_port_indev_multi_ctx_t *ctx, lv_coord_t *x, lv_coord_t *y)
{
    lisa_display_capabilities_t caps = {0};
    lv_coord_t cur_x;
    lv_coord_t cur_y;

    if (!ctx || !x || !y) {
        return;
    }

    lisa_display_get_capabilities(ctx->display_dev, &caps);

#if CONFIG_LV_POINTER_SWAP_XY
    cur_x = *y;
    cur_y = *x;
#else
    cur_x = *x;
    cur_y = *y;
#endif

#if CONFIG_LV_POINTER_INVERT_X
    cur_x = caps.width - cur_x;
#endif

#if CONFIG_LV_POINTER_INVERT_Y
    cur_y = caps.height - cur_y;
#endif

    *x = cur_x;
    *y = cur_y;
}
