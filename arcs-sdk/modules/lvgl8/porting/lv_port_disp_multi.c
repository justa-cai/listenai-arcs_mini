/**
 * @file lv_port_disp_multi.c
 */

#include "lv_port_disp_multi.h"
#include "lv_port_mem.h"
#include <stddef.h>

typedef struct {
    lisa_device_t *display_dev;
    lv_disp_draw_buf_t draw_buf;
    lv_disp_drv_t disp_drv;
    lv_disp_t *disp;
    lv_color_t *buf1;
#if CONFIG_LV_DOUBLE_VDB
    lv_color_t *buf2;
#endif
} lv_port_disp_multi_ctx_t;

static void disp_flush_multi(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

static lv_port_disp_multi_ctx_t *lv_port_disp_ctx_from_drv(lv_disp_drv_t *disp_drv)
{
    return (lv_port_disp_multi_ctx_t *)((char *)disp_drv - offsetof(lv_port_disp_multi_ctx_t, disp_drv));
}

#if CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE > 1
static void disp_rounder_multi(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    area->y1 &= ~(CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
    area->y2 |= (CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
    area->x1 &= ~(CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);
    area->x2 |= (CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE - 1);

    lv_coord_t max_x;
    lv_coord_t max_y;

    if (disp_drv->rotated == LV_DISP_ROT_90 || disp_drv->rotated == LV_DISP_ROT_270) {
        max_x = disp_drv->ver_res - 1;
        max_y = disp_drv->hor_res - 1;
    } else {
        max_x = disp_drv->hor_res - 1;
        max_y = disp_drv->ver_res - 1;
    }

    if (area->x2 > max_x) {
        area->x2 = max_x;
    }
    if (area->y2 > max_y) {
        area->y2 = max_y;
    }
}
#endif

lv_disp_t *lv_port_disp_register(lisa_device_t *display_dev)
{
    lisa_display_capabilities_t caps = {0};
    lv_port_disp_multi_ctx_t *ctx;
    uint32_t size;

    if (!display_dev) {
        return NULL;
    }

    if (lisa_display_get_capabilities(display_dev, &caps) != 0) {
        return NULL;
    }

    ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) {
        LV_LOG_ERROR("[%s] context alloc failed", __FUNCTION__);
        return NULL;
    }
    lv_memset_00(ctx, sizeof(*ctx));

    ctx->display_dev = display_dev;

    size = (uint32_t)caps.width * caps.height * sizeof(lv_color_t);
    ctx->buf1 = lvgl_port_malloc(size);
    if (!ctx->buf1) {
        LV_LOG_ERROR("[%s] buf1 alloc failed", __FUNCTION__);
        lv_mem_free(ctx);
        return NULL;
    }

#if CONFIG_LV_DOUBLE_VDB
    ctx->buf2 = lvgl_port_malloc(size);
    if (!ctx->buf2) {
        LV_LOG_ERROR("[%s] buf2 alloc failed", __FUNCTION__);
        lvgl_port_free(ctx->buf1);
        lv_mem_free(ctx);
        return NULL;
    }
    lv_disp_draw_buf_init(&ctx->draw_buf, ctx->buf1, ctx->buf2, size / sizeof(lv_color_t));
#else
    lv_disp_draw_buf_init(&ctx->draw_buf, ctx->buf1, NULL, size / sizeof(lv_color_t));
#endif

    lv_disp_drv_init(&ctx->disp_drv);
    ctx->disp_drv.hor_res = caps.width;
    ctx->disp_drv.ver_res = caps.height;
    ctx->disp_drv.draw_buf = &ctx->draw_buf;
    ctx->disp_drv.flush_cb = disp_flush_multi;
#if CONFIG_LV_DRIVER_PIXEL_ALIGN_SIZE > 1
    ctx->disp_drv.rounder_cb = disp_rounder_multi;
#endif
#if CONFIG_LV_DRIVER_ROTATE_NORMAL
    ctx->disp_drv.rotated = LV_DISP_ROT_NONE;
    lisa_display_set_orientation(display_dev, LISA_DISPLAY_ORIENTATION_0);
#elif CONFIG_LV_DRIVER_ROTATE_90
    ctx->disp_drv.rotated = LV_DISP_ROT_90;
    lisa_display_set_orientation(display_dev, LISA_DISPLAY_ORIENTATION_90);
#elif CONFIG_LV_DRIVER_ROTATE_270
    ctx->disp_drv.rotated = LV_DISP_ROT_270;
    lisa_display_set_orientation(display_dev, LISA_DISPLAY_ORIENTATION_270);
#endif

    ctx->disp = lv_disp_drv_register(&ctx->disp_drv);
    if (!ctx->disp) {
#if CONFIG_LV_DOUBLE_VDB
        lvgl_port_free(ctx->buf2);
#endif
        lvgl_port_free(ctx->buf1);
        lv_mem_free(ctx);
        return NULL;
    }

    lisa_display_blanking_off(display_dev);
    return ctx->disp;
}

static void disp_flush_multi(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    lisa_display_buffer_desc_t desc;
    lv_port_disp_multi_ctx_t *ctx = lv_port_disp_ctx_from_drv(disp_drv);

    if (!ctx || !ctx->display_dev) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    desc.width = area->x2 - area->x1 + 1;
    desc.height = area->y2 - area->y1 + 1;
    desc.pitch = desc.width * sizeof(lv_color_t);
    desc.buf_size = desc.width * desc.height * sizeof(lv_color_t);

    lisa_display_write(ctx->display_dev, area->x1, area->y1, &desc, color_p);
    lv_disp_flush_ready(disp_drv);
}
