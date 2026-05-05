/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_VIDEO_H
#define USBH_VIDEO_H

#include "usb_video.h"
#include "usb_hc.h"

#define USBH_VIDEO_FORMAT_UNCOMPRESSED 0
#define USBH_VIDEO_FORMAT_MJPEG        1

/* Number of bulk URB receive chunks per streaming session */
#ifndef CONFIG_USBH_VIDEO_BULK_CHUNK_SIZE
#define CONFIG_USBH_VIDEO_BULK_CHUNK_SIZE 512
#endif

/* Number of ISO packets per URB for ISO streaming */
#ifndef CONFIG_USBH_VIDEO_ISO_PACKETS_PER_URB
#define CONFIG_USBH_VIDEO_ISO_PACKETS_PER_URB 32
#endif

struct usbh_video_resolution {
    uint16_t wWidth;
    uint16_t wHeight;
    uint32_t dwDefaultFrameInterval;
};

struct usbh_video_format {
    struct usbh_video_resolution frame[12];
    uint8_t format_type;
    uint8_t num_of_frames;
};

/* Forward declaration so the callback typedef can reference struct usbh_video */
struct usbh_video;

/**
 * Frame callback: called once per complete UVC frame.
 * @param video_class  the video instance
 * @param frame_buf    pointer to the start of frame data (payload headers stripped)
 * @param frame_size   number of valid bytes in frame_buf
 * @param arg          user argument passed to usbh_video_start_streaming()
 */
typedef void (*usbh_video_frame_callback_t)(struct usbh_video *video_class,
                                            uint8_t *frame_buf,
                                            uint32_t frame_size,
                                            void *arg);

struct usbh_video {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *isoin;  /* ISO IN endpoint */
    struct usb_endpoint_descriptor *bulkin; /* Bulk IN endpoint */

    uint8_t ctrl_intf; /* interface number */
    uint8_t data_intf; /* interface number */
    uint8_t minor;
    struct video_probe_and_commit_controls probe;
    struct video_probe_and_commit_controls commit;
    uint16_t isoin_mps;
    bool is_opened;
    uint8_t current_format;
    bool is_bulk;
    uint16_t bcdVDC;
    uint8_t num_of_intf_altsettings;
    uint8_t num_of_formats;
    struct usbh_video_format format[3];

    /* bulk streaming state */
    bool streaming;
    uint8_t last_fid;                        /* last UVC FID bit, 0xFF = uninitialized */
    uint8_t bulk_error_count;                /* consecutive bulk URB error counter; cleared on success */
    uint8_t *frame_buf;                      /* user-provided frame assembly buffer */
    uint32_t frame_bufsize;                  /* size of frame_buf */
    uint32_t frame_offset;                   /* current write offset into frame_buf */
    uint8_t *chunk_buf;                      /* user-provided DMA staging buffer (one UVC payload) */
    uint32_t chunk_size;                     /* size of chunk_buf; equals dwMaxPayloadTransferSize */
    usbh_video_frame_callback_t frame_cb;    /* called once per complete frame */
    void *frame_cb_arg;                      /* user argument forwarded to frame_cb */
    struct usbh_urb bulkin_urb;              /* URB for bulk IN streaming */

    /* ISO streaming state */
    struct usbh_urb *isoin_urb;              /* dynamically allocated ISO URB (with iso_packet[]) */
    uint8_t *isoin_data_buf;                 /* ISO data buffer */

    void *user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_video_get(struct usbh_video *video_class, uint8_t request, uint8_t intf, uint8_t entity_id, uint8_t cs, uint8_t *buf, uint16_t len);
int usbh_video_set(struct usbh_video *video_class, uint8_t request, uint8_t intf, uint8_t entity_id, uint8_t cs, uint8_t *buf, uint16_t len);

int usbh_video_open(struct usbh_video *video_class,
                    uint8_t format_type,
                    uint16_t wWidth,
                    uint16_t wHeight,
                    uint8_t altsetting);
int usbh_video_close(struct usbh_video *video_class);

void usbh_video_list_info(struct usbh_video *video_class);

void usbh_video_run(struct usbh_video *video_class);
void usbh_video_stop(struct usbh_video *video_class);

/**
 * Start video streaming (bulk or ISO, depending on the device).
 *
 * @param video_class   video instance (must be opened via usbh_video_open first)
 * @param frame_buf     caller-provided buffer for assembled frames (DMA-safe, PSRAM ok)
 * @param frame_bufsize size of frame_buf in bytes
 * @param chunk_buf     caller-provided DMA staging buffer for one UVC payload unit (bulk mode only;
 *                      must be DMA-safe and aligned to CONFIG_USB_ALIGN_SIZE).
 *                      Ignored in ISO mode — pass NULL.
 * @param chunk_bufsize size of chunk_buf in bytes (ignored in ISO mode — pass 0)
 * @param cb            callback invoked once per complete UVC frame
 * @param arg           opaque argument forwarded to cb
 * @return 0 on success, negative on error
 */
int usbh_video_start_streaming(struct usbh_video *video_class,
                               uint8_t *frame_buf,
                               uint32_t frame_bufsize,
                               uint8_t *chunk_buf,
                               uint32_t chunk_bufsize,
                               usbh_video_frame_callback_t cb,
                               void *arg);

/**
 * Stop video streaming. Cancels the pending URB and clears streaming state.
 *
 * @param video_class   video instance
 * @return 0 on success, negative on error
 */
int usbh_video_stop_streaming(struct usbh_video *video_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_VIDEO_H */
