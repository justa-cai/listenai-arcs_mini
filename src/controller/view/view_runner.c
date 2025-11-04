#include <stdio.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "lv_demos.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lisa_log.h"
#include "lisaui.h"
#include "assistant_view.h"
#include "lisa_display.h"
#include "IOMuxManager.h"

#ifdef CONFIG_LV_CUSTOM_TASK_CPU_PER
extern void lvgl_task_cpu_percent_peroid500ms(void);
#endif
#include "app_display.h"

#define TAG "view_main"

static void lvgl_handler_work(void *param)
{
    assistant_view_t *view = (assistant_view_t *)param;
    uint32_t wait_time = lv_task_handler();
    if (wait_time > 1000) {
        printf("lvgl_handler_work wait_time: %d\n", wait_time);
        __builtin_trap();
        return;
    }
    workqueue_submit(view->workq, lvgl_handler_work, view, pdMS_TO_TICKS(wait_time));
#ifdef CONFIG_LV_CUSTOM_TASK_CPU_PER
    lvgl_task_cpu_percent_peroid500ms();
#endif
}

int view_runner_start(assistant_view_t *view)
{
    lv_init();

    display_hw_config_t disp_config = {
        .trans_config = {
            .spi_4line = {
                .spi_pins = {
                    .clk = {.pad = CSK_IOMUX_PAD_A, .pin = 25, .func = CSK_IOMUX_FUNC_ALTER6},
                    .cs =  {.pad = CSK_IOMUX_PAD_A, .pin = 22, .func = CSK_IOMUX_FUNC_DEFAULT},
                    .dc =  {.pad = CSK_IOMUX_PAD_A, .pin = 23, .func = CSK_IOMUX_FUNC_DEFAULT},
                    .sda = {.pad = CSK_IOMUX_PAD_A, .pin = 24, .func = CSK_IOMUX_FUNC_ALTER6},
                },
                .spi_tx_dma_ch = 3,
                .spi_dev = SPI1(),
            }
        },
        .blacklight = {
            .dev = GPT0_PWM(),
            .pin = {.pad = CSK_IOMUX_PAD_A, .pin = 21, .func = CSK_IOMUX_FUNC_ALTER12},
            .channel = 1,
            .freq = 1000,
        },
        .reset = {
            .pad = CSK_IOMUX_PAD_B,
            .pin = 9,
            .func = CSK_IOMUX_FUNC_DEFAULT,
        },
        .te = {
            .pad = CSK_IOMUX_PAD_A,
            .pin = 27,
            .func = CSK_IOMUX_FUNC_DEFAULT,
        },
    };
    lv_port_disp_init(&disp_config);
    lv_img_net_loader_init();
    // lv_port_indev_init();
    lisa_display_blanking_on(lisa_display_get());
    lisaui_ui_init();
    lv_obj_t *obj = lv_scr_act();
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_task_handler();
    lisa_display_blanking_off(lisa_display_get());

    workqueue_submit(view->workq, lvgl_handler_work, view, 0);
    return 0;
}
