#include "text2img_config.h"
#include "lisa_log.h"

#define TAG "text2img_config"

#define TEXT2IMG_DEFAULT_BASE_URL "http://192.168.1.169:9100"

void text2img_config_init(void)
{
    LISA_LOGI(TAG, "Text2img config initialized");
}

const char *text2img_config_get_api_key(void)
{
    return NULL;
}

const char *text2img_config_get_base_url(void)
{
    return TEXT2IMG_DEFAULT_BASE_URL;
}
