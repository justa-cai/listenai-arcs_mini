#ifndef LV_IMG_NET_LOADER_H
#define LV_IMG_NET_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Initialize the network image loader
 */
void lv_img_net_loader_init(void);

/**
 * @brief Load an image from network URL and return image descriptor
 * @param url The network URL (without N: prefix)
 * @return Pointer to image descriptor, or NULL if failed
 * @note The returned pointer should be freed with lv_img_net_free() when done
 */
lv_img_dsc_t* lv_img_net_load(const char* url);

/**
 * @brief Free a network-loaded image
 * @param img_dsc Image descriptor returned by lv_img_net_load()
 */
void lv_img_net_free(lv_img_dsc_t* img_dsc);

/**
 * @brief Set image source from network URL
 * @param img LVGL image object
 * @param url Network URL (with or without N: prefix)
 * @return LV_RES_OK on success, error code on failure
 */
lv_res_t lv_img_set_src_net(lv_obj_t* img, const char* url);

#ifdef __cplusplus
}
#endif

#endif /* LV_IMG_NET_LOADER_H */