#ifndef __UBOOT_FEATURES_H__
#define __UBOOT_FEATURES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define UBOOT_FEATURES_MAGIC    0x55424F46u    /* "UBOF" */
#define UBOOT_FEATURES_VERSION  1u

#define UBOOT_FEATURE_OTA           (1u << 0)
#define UBOOT_FEATURE_POWER_GUARD   (1u << 1)

struct uboot_features_desc {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t features;
    uint32_t reserved[4];
};

#ifdef __cplusplus
}
#endif

#endif /* __UBOOT_FEATURES_H__ */
