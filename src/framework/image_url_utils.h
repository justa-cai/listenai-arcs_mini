#ifndef IMAGE_URL_UTILS_H
#define IMAGE_URL_UTILS_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMAGE_OSS_DEFAULT_WIDTH (148)

static inline int build_image_display_url_with_width(const char *input_url,
                                                     char *output_url,
                                                     size_t output_size,
                                                     unsigned int width)
{
    char oss_process[128] = {0};

    if (!input_url || !output_url || output_size == 0 || width == 0) {
        return -1;
    }

    int p = snprintf(oss_process,
                     sizeof(oss_process),
                     "x-oss-process=image/resize,m_pad,w_%u,limit_0,color_000000/quality,q_100/format,jpg",
                     width);
    if (p <= 0 || (size_t)p >= sizeof(oss_process)) {
        return -1;
    }

    const char *q = strchr(input_url, '?');
    if (!q) {
        int n = snprintf(output_url, output_size, "%s?%s", input_url, oss_process);
        return (n > 0 && (size_t)n < output_size) ? 0 : -1;
    }

    if (strstr(q + 1, "x-oss-process=") != NULL) {
        size_t base_len = (size_t)(q - input_url);
        if (base_len + 1 >= output_size) {
            return -1;
        }
        memcpy(output_url, input_url, base_len);
        output_url[base_len] = '\0';

        int n = snprintf(output_url + base_len, output_size - base_len, "?%s", oss_process);
        return (n > 0 && (size_t)n < (output_size - base_len)) ? 0 : -1;
    }

    int n = snprintf(output_url, output_size, "%s&%s", input_url, oss_process);
    return (n > 0 && (size_t)n < output_size) ? 0 : -1;
}

static inline int build_image_display_url(const char *input_url, char *output_url, size_t output_size)
{
    return build_image_display_url_with_width(input_url,
                                              output_url,
                                              output_size,
                                              IMAGE_OSS_DEFAULT_WIDTH);
}

#ifdef __cplusplus
}
#endif

#endif
