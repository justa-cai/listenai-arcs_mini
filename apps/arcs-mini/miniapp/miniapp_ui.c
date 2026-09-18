#include "miniapp.h"

#if MINIAPP_HAS_SCREEN

#include <string.h>

#include "FreeRTOS.h"
#include "alarm_ring.h"
#include "lisa_mem.h"
#include "lisa_ui.h"
#include "lisa_ui_fonts.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lvgl.h"
#include "models/model_voice.h"
#include "semphr.h"
#include "task.h"

typedef struct {
    lv_obj_t *root;
    lv_timer_t *render_timer;
    lv_obj_t *rects[MINIAPP_MAX_RECTS];
    lv_obj_t *texts[MINIAPP_MAX_TEXTS];
    uint16_t rect_count;
    uint16_t text_count;
    uint32_t applied_sequence;
} miniapp_page_t;

static miniapp_page_t *s_page;
static SemaphoreHandle_t s_nav_done;
static int s_nav_result;
static uint32_t s_nav_generation;
static uint32_t s_nav_completed;
static SemaphoreHandle_t s_scene_lock;
// A complete scene is just over 2 KB. Keep both handoff buffers in PSRAM so
// enabling miniapps does not consume the default board's scarce static SRAM.
static miniapp_scene_t *s_pending_scene;
static miniapp_scene_t *s_render_scene;
static uint32_t s_scene_sequence;
static uint32_t s_applied_sequence;
static bool s_page_open;

static void miniapp_apply_scene(miniapp_page_t *page, const miniapp_scene_t *scene)
{
    lv_obj_set_style_bg_color(page->root, lv_color_hex(scene->background), 0);
    for (size_t i = 0; i < scene->rect_count; ++i) {
        lv_obj_t *obj = page->rects[i];
        const miniapp_rect_t *rect = &scene->rects[i];
        lv_obj_set_pos(obj, rect->x, rect->y);
        lv_obj_set_size(obj, rect->w, rect->h);
        lv_obj_set_style_bg_color(obj, lv_color_hex(rect->color), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    for (size_t i = scene->rect_count; i < page->rect_count; ++i) {
        lv_obj_add_flag(page->rects[i], LV_OBJ_FLAG_HIDDEN);
    }
    page->rect_count = scene->rect_count;

    for (size_t i = 0; i < scene->text_count; ++i) {
        lv_obj_t *label = page->texts[i];
        const miniapp_text_t *text = &scene->texts[i];
        lv_obj_set_pos(label, text->x, text->y);
        lv_obj_set_style_text_color(label, lv_color_hex(text->color), 0);
        lv_label_set_text(label, text->text);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    for (size_t i = scene->text_count; i < page->text_count; ++i) {
        lv_obj_add_flag(page->texts[i], LV_OBJ_FLAG_HIDDEN);
    }
    page->text_count = scene->text_count;
}

static void miniapp_render_timer_cb(lv_timer_t *timer)
{
    miniapp_page_t *page = timer ? timer->user_data : NULL;
    miniapp_scene_t *scene = s_render_scene;
    uint32_t sequence;

    if (!page || !scene || !s_pending_scene || !s_scene_lock ||
        xSemaphoreTake(s_scene_lock, 0) != pdTRUE) {
        return;
    }
    sequence = s_scene_sequence;
    if (sequence == page->applied_sequence) {
        xSemaphoreGive(s_scene_lock);
        return;
    }
    *scene = *s_pending_scene;
    xSemaphoreGive(s_scene_lock);

    miniapp_apply_scene(page, scene);
    page->applied_sequence = sequence;

    xSemaphoreTake(s_scene_lock, portMAX_DELAY);
    s_applied_sequence = sequence;
    xSemaphoreGive(s_scene_lock);
}

static int miniapp_page_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    (void)scr;
    miniapp_page_t *page = lisa_ui_malloc(sizeof(*page));
    if (!page) {
        return -1;
    }
    memset(page, 0, sizeof(*page));
    page->root = lv_obj_create(lv_scr_act());
    if (!page->root) {
        lisa_ui_free(page);
        return -1;
    }
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(page->root, 0, 0);
    lv_obj_set_size(page->root, MINIAPP_SCREEN_WIDTH, MINIAPP_SCREEN_HEIGHT);
    lv_obj_set_style_pad_all(page->root, 0, 0);
    lv_obj_set_style_border_width(page->root, 0, 0);
    lv_obj_set_style_radius(page->root, 0, 0);
    lv_obj_set_style_bg_color(page->root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(page->root, LV_OPA_COVER, 0);

    for (size_t i = 0; i < MINIAPP_MAX_RECTS; ++i) {
        page->rects[i] = lv_obj_create(page->root);
        if (!page->rects[i]) {
            lv_obj_del(page->root);
            lisa_ui_free(page);
            return -1;
        }
        lv_obj_clear_flag(page->rects[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_width(page->rects[i], 0, 0);
        lv_obj_set_style_radius(page->rects[i], 0, 0);
        lv_obj_set_style_pad_all(page->rects[i], 0, 0);
        lv_obj_add_flag(page->rects[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (size_t i = 0; i < MINIAPP_MAX_TEXTS; ++i) {
        page->texts[i] = lv_label_create(page->root);
        if (!page->texts[i]) {
            lv_obj_del(page->root);
            lisa_ui_free(page);
            return -1;
        }
        lv_obj_set_style_text_font(page->texts[i], &lv_font_chinese_16, 0);
        lv_obj_set_style_text_align(page->texts[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_add_flag(page->texts[i], LV_OBJ_FLAG_HIDDEN);
    }
    page->render_timer = lv_timer_create(miniapp_render_timer_cb, 20, page);
    if (!page->render_timer) {
        lv_obj_del(page->root);
        lisa_ui_free(page);
        return -1;
    }
    s_page = page;
    xSemaphoreTake(s_scene_lock, portMAX_DELAY);
    s_page_open = true;
    xSemaphoreGive(s_scene_lock);
    lv_timer_ready(page->render_timer);
    model_voice_on();
    *data = page;
    return 0;
}

static int miniapp_page_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    miniapp_page_t *page = data;
    s_page = NULL;
    if (page) {
        if (page->render_timer) {
            lv_timer_del(page->render_timer);
            page->render_timer = NULL;
        }
        if (page->root) {
            lv_obj_del(page->root);
        }
        lisa_ui_free(page);
    }
    xSemaphoreTake(s_scene_lock, portMAX_DELAY);
    s_page_open = false;
    xSemaphoreGive(s_scene_lock);
    miniapp_runtime_ui_closed();
    return 0;
}

static int miniapp_page_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    miniapp_page_t *page = data;
    lv_timer_pause(page->render_timer);
    lv_obj_add_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int miniapp_page_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    (void)scr;
    miniapp_page_t *page = data;
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    lv_timer_resume(page->render_timer);
    lv_timer_ready(page->render_timer);
    model_voice_on();
    return 0;
}

static const struct lisa_ui_nav_scr s_miniapp_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_MINIAPP,
    .open = miniapp_page_open,
    .close = miniapp_page_close,
    .pause = miniapp_page_pause,
    .resume = miniapp_page_resume,
};

static void miniapp_nav_open_worker(void *arg, uint32_t len)
{
    if (!arg || len != sizeof(uint32_t)) return;
    uint32_t generation = *(uint32_t *)arg;
    taskENTER_CRITICAL();
    bool current = generation == s_nav_generation;
    taskEXIT_CRITICAL();
    if (!current) return;
    int result;
    if (alarm_ring_is_active() || lv_disp_get_hor_res(NULL) != MINIAPP_SCREEN_WIDTH ||
        lv_disp_get_ver_res(NULL) != MINIAPP_SCREEN_HEIGHT) {
        result = -1;
    } else if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_MINIAPP) {
        result = 0;
    } else {
        result = lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_MINIAPP);
    }
    if (result == 0 && s_page) {
        /* home_nav_scr_pause() may have queued model_voice_off() before the
         * miniapp page was opened. Re-assert the miniapp foreground voice
         * state after navigation so that queued UI lifecycle work cannot
         * leave can_wakeup disabled. */
        model_voice_on();
        miniapp_render_timer_cb(s_page->render_timer);
    }
    taskENTER_CRITICAL();
    s_nav_completed = generation;
    s_nav_result = result;
    taskEXIT_CRITICAL();
    xSemaphoreGive(s_nav_done);
}

static void miniapp_nav_close_worker(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;
    if (!miniapp_is_active() && lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_MINIAPP) {
        (void)lisa_ui_nav_scr_nav_default();
    }
}

int miniapp_ui_init(void)
{
    if (s_scene_lock) return 0;
    s_nav_done = xSemaphoreCreateBinary();
    if (!s_nav_done) {
        return -1;
    }
    s_pending_scene = lisa_mem_calloc(1, sizeof(*s_pending_scene));
    s_render_scene = lisa_mem_calloc(1, sizeof(*s_render_scene));
    if (!s_pending_scene || !s_render_scene) {
        lisa_mem_free(s_pending_scene);
        lisa_mem_free(s_render_scene);
        s_pending_scene = NULL;
        s_render_scene = NULL;
        vSemaphoreDelete(s_nav_done);
        s_nav_done = NULL;
        return -1;
    }
    s_scene_lock = xSemaphoreCreateMutex();
    if (!s_scene_lock) {
        lisa_mem_free(s_pending_scene);
        lisa_mem_free(s_render_scene);
        s_pending_scene = NULL;
        s_render_scene = NULL;
        vSemaphoreDelete(s_nav_done);
        s_nav_done = NULL;
        return -1;
    }
    int result = lisa_ui_nav_scr_add(&s_miniapp_nav_scr);
    if (result != 0) {
        vSemaphoreDelete(s_scene_lock);
        vSemaphoreDelete(s_nav_done);
        lisa_mem_free(s_pending_scene);
        lisa_mem_free(s_render_scene);
        s_scene_lock = NULL;
        s_nav_done = NULL;
        s_pending_scene = s_render_scene = NULL;
    }
    return result;
}

int miniapp_ui_open(void)
{
    taskENTER_CRITICAL();
    uint32_t generation = ++s_nav_generation;
    taskEXIT_CRITICAL();
    (void)xSemaphoreTake(s_nav_done, 0);
    if (lisa_ui_invoke_ui_delayed(miniapp_nav_open_worker, &generation, sizeof(generation), 0) != 0) {
        return -1;
    }
    TickType_t start = xTaskGetTickCount();
    TickType_t remaining = pdMS_TO_TICKS(2000);
    while (xSemaphoreTake(s_nav_done, remaining) == pdTRUE) {
        taskENTER_CRITICAL();
        bool completed = s_nav_completed == generation;
        int result = s_nav_result;
        taskEXIT_CRITICAL();
        if (completed) return result;
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= pdMS_TO_TICKS(2000)) break;
        remaining = pdMS_TO_TICKS(2000) - elapsed;
    }
    taskENTER_CRITICAL();
    ++s_nav_generation; /* Cancel an open that has not reached the UI yet. */
    taskEXIT_CRITICAL();
    return -1;
}

int miniapp_ui_close(void)
{
    return lisa_ui_invoke_ui_delayed(miniapp_nav_close_worker, NULL, 0, 0);
}

int miniapp_ui_present(const miniapp_scene_t *scene)
{
    if (!scene || !s_pending_scene || !s_scene_lock) {
        return -1;
    }

    xSemaphoreTake(s_scene_lock, portMAX_DELAY);
    *s_pending_scene = *scene;
    s_scene_sequence++;
    xSemaphoreGive(s_scene_lock);
    return 0;
}

int miniapp_ui_wait_frame(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    do {
        xSemaphoreTake(s_scene_lock, portMAX_DELAY);
        bool ready = s_page_open && s_applied_sequence == s_scene_sequence;
        xSemaphoreGive(s_scene_lock);
        if (ready) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (xTaskGetTickCount() - start < pdMS_TO_TICKS(timeout_ms));
    return -1;
}
#else
int miniapp_ui_init(void) { return 0; }
int miniapp_ui_open(void) { return 0; }
int miniapp_ui_close(void) { return 0; }
int miniapp_ui_present(const miniapp_scene_t *scene) { (void)scene; return -1; }
int miniapp_ui_wait_frame(uint32_t timeout_ms) { (void)timeout_ms; return 0; }
#endif
