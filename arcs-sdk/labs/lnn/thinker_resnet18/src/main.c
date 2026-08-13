#define LOG_TAG "thinker_resnet18"
#include <lisa_log.h>

#include <stdint.h>

#include "resnet18.h"
#include "test_image_apple.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *label = NULL;
    int8_t score = 0;

    if (resnet18_init() != 0) {
        LOGE("resnet18 init failed");
        return -1;
    }

    if (resnet18_process(test_image, TEST_IMAGE_WIDTH, TEST_IMAGE_HEIGHT, false, &label, &score) != 0) {
        LOGE("resnet18 process failed");
        (void)resnet18_deinit();
        return -1;
    }

    LOGI("resnet18 result: label=%s score=%d", label, score);

    (void)resnet18_deinit();

    return 0;
}
