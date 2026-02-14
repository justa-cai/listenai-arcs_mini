#include "lisa_log.h"

#ifndef __TEXT2IMG_CONFIG_H__
#define __TEXT2IMG_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

void text2img_config_init(void);
const char *text2img_config_get_api_key(void);
const char *text2img_config_get_base_url(void);

#ifdef __cplusplus
}
#endif

#endif /* __TEXT2IMG_CONFIG_H__ */
