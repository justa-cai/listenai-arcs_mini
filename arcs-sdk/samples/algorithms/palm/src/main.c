#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "acomp.h"
#include "ic_message.h"

#include "app_palm.h"
#include "app_palm_image_input.h"
#include "test_image.h"

#define TAG "main"
#include "lisa_log.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int ret;
    uint32_t frame_index = 0U;

    LISA_LOGI(TAG, "CP=======! Hard ID: %d", CONFIG_HARTID);

    ic_message_init();
    LISA_LOGI(TAG, "ic_message_init done!");

    vTaskDelay(pdMS_TO_TICKS(2000));

    acomp_init();

    ret = app_palm_init();
    if (ret != 0) {
        return ret;
    }

    ret = app_palm_image_stream_init();
    if (ret != 0) {
        return ret;
    }

    palm_image_frame_t image_frame = {
        .image_data = (uint8_t *)palm_320_240_bin,
        .image_index = 0,
        .image_length = palm_320_240_bin_len,
        .image_width = 320,
        .image_height = 240,
        .image_format = ACOMP_PALM_PIX_FMT_BGR888,
    };

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        image_frame.image_index = frame_index++;
        (void)app_palm_image_send(&image_frame);
    }

    return 0;
}
