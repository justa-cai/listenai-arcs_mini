#include "text2img_config.h"
#include "lisa_log.h"

#define TAG "text2img_config"

// 从 Kconfig 配置获取服务器地址，格式化为完整的 URL
#ifndef CONFIG_MY_CLOUD_HOST
#define CONFIG_MY_CLOUD_HOST "192.168.1.169"
#endif
#define TEXT2IMG_DEFAULT_BASE_URL "http://" CONFIG_MY_CLOUD_HOST ":9100"

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
