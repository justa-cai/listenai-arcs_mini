/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_UAC_STREAM_H
#define CHERRYUSB_UAC_STREAM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UAC_STREAM_CAPTURE,
    UAC_STREAM_PLAYBACK,
} uac_stream_dir_t;

typedef struct uac_stream uac_stream_t;

typedef struct {
    int (*start)(uac_stream_t *stream);
    void (*stop)(uac_stream_t *stream);
    uint32_t (*read)(uac_stream_t *stream, uint8_t *buf, uint32_t bytes);
    uint32_t (*write)(uac_stream_t *stream, const uint8_t *buf, uint32_t bytes);

    void (*set_mute)(uac_stream_t *stream, uint8_t channel, bool mute);
    bool (*get_mute)(uac_stream_t *stream, uint8_t channel);
    void (*set_volume)(uac_stream_t *stream, uint8_t channel, int volume_db);
    int (*get_volume)(uac_stream_t *stream, uint8_t channel);
    void (*set_sampling_freq)(uac_stream_t *stream, uint32_t sampling_freq);
    uint32_t (*get_sampling_freq)(uac_stream_t *stream);
} uac_stream_ops_t;

struct uac_stream {
    const char *name;
    uac_stream_dir_t dir;
    const uac_stream_ops_t *ops;
    void *priv;
};

#endif /* CHERRYUSB_UAC_STREAM_H */
