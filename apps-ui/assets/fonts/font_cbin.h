#ifndef FONT_CBIN_H
#define FONT_CBIN_H

#include <stdint.h>
#include <stdbool.h>

#include "lvgl.h"

typedef struct {
    const char *path;
    lv_font_t *font;
    bool load_attempted;
} font_cbin_cache_t;

lv_font_t *font_cbin_load(const char *path);
lv_font_t *font_cbin_get(font_cbin_cache_t *cache);

#define FONT_CBIN_CACHE_INIT(path_) \
    {                               \
        .path = (path_),            \
        .font = NULL,               \
        .load_attempted = false,    \
    }

#endif /* FONT_CBIN_H */
