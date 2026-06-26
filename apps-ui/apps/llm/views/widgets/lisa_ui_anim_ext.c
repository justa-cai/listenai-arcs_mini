#include "esp_heap_caps.h"
#include "lisa_ui_anim_ext.h"
#include <stdint.h>
#include <string.h>

static void lisa_ui_anim_ext_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_anim_ext_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_anim_ext_class = {
    .constructor_cb = lisa_ui_anim_ext_class_constructor,
    .destructor_cb = lisa_ui_anim_ext_class_destructor,
    .event_cb = NULL,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lisa_ui_anim_ext_t),
    .base_class = &lisa_ui_anim_class,
};

#define MY_CLASS &lisa_ui_anim_ext_class

#define PNG_DECODE_RGBA_BPP       4U
#define PNG_DECODE_RESERVE_BYTES  (64U * 1024U)
#define PNG_COLOR_TYPE_GREY       0U
#define PNG_COLOR_TYPE_RGB        2U
#define PNG_COLOR_TYPE_PALETTE    3U
#define PNG_COLOR_TYPE_GREY_ALPHA 4U
#define PNG_COLOR_TYPE_RGBA       6U

static uint64_t max_u64(uint64_t a, uint64_t b)
{
    return a > b ? a : b;
}

static bool is_png_frame(const lv_img_dsc_t *frame)
{
    static const uint8_t png_magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};

    return frame != NULL && frame->data != NULL && frame->data_size >= sizeof(png_magic) &&
           memcmp(frame->data, png_magic, sizeof(png_magic)) == 0;
}

static bool png_color_bits_per_pixel(uint8_t bit_depth, uint8_t color_type, uint32_t *bits_per_pixel)
{
    uint32_t channels;

    switch (color_type) {
    case PNG_COLOR_TYPE_GREY:
    case PNG_COLOR_TYPE_PALETTE:
        channels = 1;
        break;
    case PNG_COLOR_TYPE_RGB:
        channels = 3;
        break;
    case PNG_COLOR_TYPE_GREY_ALPHA:
        channels = 2;
        break;
    case PNG_COLOR_TYPE_RGBA:
        channels = 4;
        break;
    default:
        return false;
    }

    *bits_per_pixel = bit_depth * channels;
    return *bits_per_pixel > 0;
}

static bool get_png_frame_heap_need(const lv_img_dsc_t *frame, size_t *need_single, size_t *need_free)
{
    if (need_single) {
        *need_single = 0;
    }
    if (need_free) {
        *need_free = 0;
    }

    if (!is_png_frame(frame)) {
        return true;
    }

    if (frame->data_size < 33 || frame->header.w == 0 || frame->header.h == 0) {
        return false;
    }

    const uint8_t *png = frame->data;
    uint8_t bit_depth = png[24];
    uint8_t color_type = png[25];
    uint8_t interlace_method = png[28];
    uint32_t src_bpp;

    if (!png_color_bits_per_pixel(bit_depth, color_type, &src_bpp)) {
        return false;
    }

    uint64_t width = frame->header.w;
    uint64_t height = frame->header.h;
    uint64_t pixel_count = width * height;
    uint64_t rgba_size = pixel_count * PNG_DECODE_RGBA_BPP;
    uint64_t src_raw_size = (pixel_count * src_bpp + 7U) / 8U;
    uint64_t row_size = (width * src_bpp + 7U) / 8U;
    uint64_t scanline_size = height * (row_size + 1U);

    if (interlace_method != 0) {
        scanline_size = src_raw_size + height * 8U;
    }

    uint64_t compressed_size = frame->data_size;
    uint64_t max_single = max_u64(compressed_size, max_u64(scanline_size, max_u64(src_raw_size, rgba_size)));
    uint64_t peak_size = max_u64(compressed_size + scanline_size, scanline_size + src_raw_size);

    if (src_bpp != PNG_DECODE_RGBA_BPP * 8U || bit_depth != 8U) {
        peak_size = max_u64(peak_size, src_raw_size + rgba_size);
    }

    uint64_t need_free64 = peak_size + PNG_DECODE_RESERVE_BYTES;
    uint64_t size_max = (uint64_t)((size_t)-1);

    if (max_single > size_max || need_free64 > size_max) {
        return false;
    }

    if (need_single) {
        *need_single = (size_t)max_single;
    }
    if (need_free) {
        *need_free = (size_t)need_free64;
    }

    return true;
}

static bool collect_anim_config_heap_need(const lisa_ui_anim_config_t *config, lisa_ui_anim_heap_check_t *check)
{
    if (config == NULL || check == NULL || config->frames == NULL || config->frame_count == 0) {
        return true;
    }

    for (uint16_t i = 0; i < config->frame_count; i++) {
        size_t frame_need_single;
        size_t frame_need_free;

        if (!get_png_frame_heap_need(&config->frames[i], &frame_need_single, &frame_need_free)) {
            return false;
        }

        if (frame_need_single > check->need_single) {
            check->need_single = frame_need_single;
        }
        if (frame_need_free > check->need_free) {
            check->need_free = frame_need_free;
        }
    }

    return true;
}

static void lisa_ui_anim_stop_cb_handle(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    if (anim_ext->state == LISA_UI_ANIM_STATE_ENTER) {
        anim_ext->state = LISA_UI_ANIM_STATE_LOOP;
        lisa_ui_anim_set_config(obj, &anim_ext->curr.loop);
        lisa_ui_anim_start(obj);
    } else if (anim_ext->state == LISA_UI_ANIM_STATE_LOOP) {
        anim_ext->state = LISA_UI_ANIM_STATE_STOPING;
        if (anim_ext->curr.exit.frame_count != 0) {
            lisa_ui_anim_set_config(obj, &anim_ext->curr.exit);
            lisa_ui_anim_start(obj);
        }
    } else if (anim_ext->state == LISA_UI_ANIM_STATE_NEXT_REQ) {
        lisa_ui_anim_ext_set_config(obj, &anim_ext->next);
        lisa_ui_anim_ext_start(obj);
    } else {
        anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
    }
}

static void lisa_ui_anim_loop_cb_handle(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    if (anim_ext->state == LISA_UI_ANIM_STATE_STOP_REQ) {
        if (anim_ext->curr.exit.frame_count != 0) {
            anim_ext->state = LISA_UI_ANIM_STATE_STOPING;
            lisa_ui_anim_set_config(obj, &anim_ext->curr.exit);
            lisa_ui_anim_start(obj);
        } else {
            anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
            lisa_ui_anim_stop(obj);
        }
    } else if (anim_ext->state == LISA_UI_ANIM_STATE_NEXT_REQ) {
        if (anim_ext->curr.exit.frame_count != 0) {
            lisa_ui_anim_set_config(obj, &anim_ext->curr.exit);
            lisa_ui_anim_start(obj);
        } else {
            lisa_ui_anim_ext_set_config(obj, &anim_ext->next);
            lisa_ui_anim_ext_start(obj);
        }
    }
}

static void lisa_ui_anim_ext_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    memset(&anim_ext->curr, 0, sizeof(lisa_ui_anim_config_t));
    memset(&anim_ext->next, 0, sizeof(lisa_ui_anim_config_t));
    anim_ext->state = LISA_UI_ANIM_STATE_NONE;

    lisa_ui_anim_stop_cb_set(obj, lisa_ui_anim_stop_cb_handle);
    lisa_ui_anim_loop_cb_set(obj, lisa_ui_anim_loop_cb_handle);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lisa_ui_anim_ext_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    LV_TRACE_OBJ_CREATE("finished");
}

lv_obj_t *lisa_ui_anim_ext_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

void lisa_ui_anim_ext_set_config(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *config)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_ASSERT(config != NULL);

    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    memcpy(&anim_ext->curr.enter, &config->enter, sizeof(lisa_ui_anim_config_t));
    memcpy(&anim_ext->curr.loop, &config->loop, sizeof(lisa_ui_anim_config_t));
    memcpy(&anim_ext->curr.exit, &config->exit, sizeof(lisa_ui_anim_config_t));
}

bool lisa_ui_anim_ext_config_has_enough_heap(const lisa_ui_anim_ext_config_t *config,
                                             lisa_ui_anim_heap_check_t *check)
{
    lisa_ui_anim_heap_check_t local_check = {0};
    lisa_ui_anim_heap_check_t *result = check ? check : &local_check;

    memset(result, 0, sizeof(*result));

    if (config == NULL) {
        return false;
    }

    if (!collect_anim_config_heap_need(&config->enter, result) ||
        !collect_anim_config_heap_need(&config->loop, result) ||
        !collect_anim_config_heap_need(&config->exit, result)) {
        return false;
    }

    if (result->need_single == 0 && result->need_free == 0) {
        return true;
    }

    result->largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_PID2);
    result->free_size = heap_caps_get_free_size(MALLOC_CAP_PID2);

    return result->largest_free >= result->need_single && result->free_size >= result->need_free;
}

void lisa_ui_anim_ext_start(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    lisa_ui_anim_config_t *cfg = &anim_ext->curr.enter;

    if (cfg->frame_count == 0) {
        cfg = &anim_ext->curr.loop;
        anim_ext->state = LISA_UI_ANIM_STATE_LOOP;
    } else {
        anim_ext->state = LISA_UI_ANIM_STATE_ENTER;
    }

    lisa_ui_anim_set_config(obj, cfg);
    lisa_ui_anim_start(obj);
}

void lisa_ui_anim_ext_stop(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    if (anim_ext->state == LISA_UI_ANIM_STATE_NONE || anim_ext->state == LISA_UI_ANIM_STATE_STOPED) {
        return;
    }

    if (anim_ext->curr.exit.frame_count == 0) {
        anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
        lisa_ui_anim_stop(obj);
        return;
    }

    anim_ext->state = LISA_UI_ANIM_STATE_STOP_REQ;
}

void lisa_ui_anim_ext_next(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *next)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    if ((anim_ext->state != LISA_UI_ANIM_STATE_STOPED) && (anim_ext->state != LISA_UI_ANIM_STATE_NONE)) {
        memcpy(&anim_ext->next, next, sizeof(lisa_ui_anim_ext_config_t));
        anim_ext->state = LISA_UI_ANIM_STATE_NEXT_REQ;
    } else {
        lisa_ui_anim_ext_set_config(obj, next);
        lisa_ui_anim_ext_start(obj);
    }
}

void lisa_ui_anim_ext_next_imm(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *next)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    lisa_ui_anim_ext_exit(obj);
    lisa_ui_anim_ext_next(obj, next);
}

void lisa_ui_anim_ext_exit(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
    lisa_ui_anim_stop(obj);
}
