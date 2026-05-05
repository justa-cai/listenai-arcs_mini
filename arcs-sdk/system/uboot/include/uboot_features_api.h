#ifndef __UBOOT_FEATURES_API_H__
#define __UBOOT_FEATURES_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "uboot_features.h"

/* 返回 boot 在 CONFIG_BOOT_FEATURES_ADDR 声明的特性位。老 boot 无 descriptor
 * 时读到的 magic 不匹配，返回 0，上层据此回退到自带实现。结果单次启动内缓存。 */
uint32_t uboot_features_query(void);

bool uboot_features_has(uint32_t feature);

#ifdef __cplusplus
}
#endif

#endif /* __UBOOT_FEATURES_API_H__ */
