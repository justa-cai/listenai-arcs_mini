#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"

#include "ic_message.h"

#include "app_fd.h"
#include "app_fd_image_input.h"

#include "test_image.h"

#define TAG "main"

#include "lisa_log.h"

int main(int argc, char **argv)
{
    printf("CP=======! Hard ID: %d\n", CONFIG_HARTID);

    ic_message_init();
    LISA_LOGI(TAG, "ic_message_init done!");

    vTaskDelay(pdMS_TO_TICKS(2000));

    /* 算法组件初始化 */
    acomp_init();
    
    /* 人脸识别组件初始化 */
    app_fd_init();

    /* 人脸识别组件输入图片数据流初始化 */
    app_fd_image_stream_init();

    image_frame_t image_frame = {
        .image_data = (uint8_t *)yuyv422_320_240,
        .image_index = 0,
        .image_length = yuyv422_320_240_len,
        .image_width = 320,
        .image_height = 240,
        .image_format = PIX_FMT_YUV422_YUYV_PACKED,
    };

    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
        /* 给算法组件输入图片数据 */
        app_fd_image_send(&image_frame);
    }

    return 0;
}
