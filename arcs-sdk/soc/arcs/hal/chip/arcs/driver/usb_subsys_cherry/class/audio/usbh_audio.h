/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_AUDIO_H
#define USBH_AUDIO_H

#include "usb_audio.h"

#ifndef CONFIG_USBHOST_AUDIO_MAX_STREAMS
#define CONFIG_USBHOST_AUDIO_MAX_STREAMS 3
#endif

#ifndef CONFIG_USBH_AUDIO_ISO_PACKETS_PER_URB
#define CONFIG_USBH_AUDIO_ISO_PACKETS_PER_URB 32
#endif

struct usbh_audio;

/* ISO IN: called when data is received (recording) */
typedef void (*usbh_audio_data_callback_t)(struct usbh_audio *audio_class,
                                           uint8_t *data, uint32_t len, void *arg);

/* ISO OUT: called to fill buffer before sending (playback).
 * App must write exactly 'len' bytes into 'buf'. */
typedef void (*usbh_audio_playback_callback_t)(struct usbh_audio *audio_class,
                                               uint8_t *buf, uint32_t len, void *arg);

struct usbh_audio_ac_msg {
    struct audio_cs_if_ac_input_terminal_descriptor ac_input;
    struct audio_cs_if_ac_feature_unit_descriptor ac_feature_unit;
    struct audio_cs_if_ac_output_terminal_descriptor ac_output;
};

struct usbh_audio_as_msg {
    const char *stream_name;
    uint8_t stream_intf;
    uint8_t input_terminal_id;
    uint8_t feature_terminal_id;
    uint8_t output_terminal_id;
    uint8_t ep_attr;
    uint8_t num_of_altsetting;
    uint16_t volume_min;
    uint16_t volume_max;
    uint16_t volume_res;
    uint16_t volume_cur;
    bool mute;
    struct audio_cs_if_as_general_descriptor as_general;
    struct audio_cs_if_as_format_type_descriptor as_format[CONFIG_USBHOST_MAX_INTF_ALTSETTINGS];
};

struct usbh_audio {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *isoin;  /* ISO IN endpoint */
    struct usb_endpoint_descriptor *isoout; /* ISO OUT endpoint */

    uint8_t ctrl_intf; /* interface number */
    uint8_t minor;
    uint16_t isoin_mps;
    uint16_t isoout_mps;
    bool is_opened;
    uint16_t bcdADC;
    uint8_t bInCollection;
    uint8_t stream_intf_num;
    struct usbh_audio_as_msg as_msg_table[CONFIG_USBHOST_AUDIO_MAX_STREAMS];

    void *user_data;

    /* ISO IN streaming state (recording) */
    bool streaming_in;
    struct usbh_urb *isoin_urb;
    uint8_t *isoin_data_buf;
    usbh_audio_data_callback_t data_cb;
    void *data_cb_arg;

    /* ISO OUT streaming state (playback) */
    bool streaming_out;
    struct usbh_urb *isoout_urb;
    uint8_t *isoout_data_buf;
    usbh_audio_playback_callback_t playback_cb;
    void *playback_cb_arg;
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_audio_open(struct usbh_audio *audio_class, const char *name, uint32_t samp_freq, uint8_t bitresolution);
int usbh_audio_close(struct usbh_audio *audio_class, const char *name);
int usbh_audio_set_volume(struct usbh_audio *audio_class, const char *name, uint8_t ch, int volume_db);
int usbh_audio_set_mute(struct usbh_audio *audio_class, const char *name, uint8_t ch, bool mute);

int usbh_audio_start_streaming(struct usbh_audio *audio_class,
                               usbh_audio_data_callback_t cb, void *arg);
int usbh_audio_start_playback(struct usbh_audio *audio_class,
                               usbh_audio_playback_callback_t cb, void *arg);
int usbh_audio_stop_streaming(struct usbh_audio *audio_class);

void usbh_audio_list_module(struct usbh_audio *audio_class);

void usbh_audio_run(struct usbh_audio *audio_class);
void usbh_audio_stop(struct usbh_audio *audio_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_AUDIO_H */
