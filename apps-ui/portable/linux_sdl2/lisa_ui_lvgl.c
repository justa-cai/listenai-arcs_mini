#define _DEFAULT_SOURCE
#include <unistd.h>

#define SDL_MAIN_HANDLED /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL2/SDL.h>

#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"

#include "config.h"
#include <stdio.h>

#include "lisa_ui.h"

static void hal_init(void);
static int tick_thread(void *data);

int lisa_ui_lvgl_run(void)
{
    while (1) {
        lv_timer_handler();
        usleep(5 * 1000);
    }
}

int lisa_ui_lvgl_init(void)
{
    /*Initialize LVGL*/
    lv_init();

    /*Initialize the HAL (display, input devices, tick) for LVGL*/
    hal_init();

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Hardware Abstraction Layer (HAL) for LVGL
 */
static void hal_init(void)
{
    /* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
    sdl_init();
    SDL_CreateThread(tick_thread, "tick", NULL);

    /*Create a display buffer*/
    static lv_color_t buf[SDL_HOR_RES * SDL_VER_RES];
    static lv_disp_draw_buf_t disp_draw_buf;
    lv_disp_draw_buf_init(&disp_draw_buf, buf, NULL, SDL_HOR_RES * SDL_VER_RES);

    /*Create a display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv); /*Basic initialization*/
    disp_drv.draw_buf = &disp_draw_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;
    lv_disp_drv_register(&disp_drv);

    /* Add a mouse as input device */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    lv_obj_t * scr = lv_obj_create(NULL);
    lv_scr_load(scr);

    LISA_UI_LOGI("SDL_HOR_RES: %d, SDL_VER_RES: %d", SDL_HOR_RES, SDL_VER_RES);
}

/**
 * A task to measure the elapsed time for LVGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void *data)
{
    (void)data;

    while (1) {
        SDL_Delay(5);
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }

    return 0;
}
