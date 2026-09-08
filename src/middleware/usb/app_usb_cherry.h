#ifndef APP_USB_CHERRY_H
#define APP_USB_CHERRY_H

#include <stdbool.h>
#include <stdint.h>

#include "app_usb_role_detect.h"

#define APP_USB_BUS_ID 0U

#define APP_USB_MSC_OUT_EP 0x01U
#define APP_USB_MSC_IN_EP  0x81U

#define APP_USB_ADB_OUT_EP 0x02U
#define APP_USB_ADB_IN_EP  0x82U

/* Use EP4; the ARCS MUSB glue reserves EP5/6/7 for 64-byte endpoints. */
#define APP_USB_AUDIO_IN_EP           0x84U
#define APP_USB_AUDIO_STREAM_ITF      2U
#define APP_USB_AUDIO_CHANNELS        4U
#define APP_USB_AUDIO_SAMPLE_RATE     16000U
#define APP_USB_AUDIO_SAMPLE_BITS     16U
#define APP_USB_AUDIO_PACKET_BYTES    128U
#define APP_USB_AUDIO_MAX_PACKET_SIZE APP_USB_AUDIO_PACKET_BYTES

bool app_usb_msc_enabled(void);
app_usb_role_t app_usb_active_role(void);
bool app_usb_host_device_enumerated(void);
void app_usb_prepare_reboot(void);
int app_usb_suspend(void);
int app_usb_resume(void);
void app_usb_cherry_descriptors_register(bool msc_mode);

#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
int app_usb_audio_run(void);
void app_usb_cherry_audio_register_endpoint(uint8_t busid);
void app_usb_cherry_audio_on_disconnect(void);
#endif

#endif /* APP_USB_CHERRY_H */
