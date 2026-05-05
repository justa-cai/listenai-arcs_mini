/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_audio.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbh_audio"
#include "usb_log.h"

#define DEV_FORMAT "/dev/audio%d"

/* general descriptor field offsets */
#define DESC_bLength            0 /** Length offset */
#define DESC_bDescriptorType    1 /** Descriptor type offset */
#define DESC_bDescriptorSubType 2 /** Descriptor subtype offset */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber  2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting 3 /** Alternate setting offset */

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_audio_buf[USB_ALIGN_UP(128, CONFIG_USB_ALIGN_SIZE)];

static struct usbh_audio g_audio_class[CONFIG_USBHOST_MAX_AUDIO_CLASS];
static uint32_t g_devinuse = 0;

static struct usbh_audio *usbh_audio_class_alloc(void)
{
    uint8_t devno;

    for (devno = 0; devno < CONFIG_USBHOST_MAX_AUDIO_CLASS; devno++) {
        if ((g_devinuse & (1U << devno)) == 0) {
            g_devinuse |= (1U << devno);
            memset(&g_audio_class[devno], 0, sizeof(struct usbh_audio));
            g_audio_class[devno].minor = devno;
            return &g_audio_class[devno];
        }
    }
    return NULL;
}

static void usbh_audio_class_free(struct usbh_audio *audio_class)
{
    uint8_t devno = audio_class->minor;

    if (devno < 32) {
        g_devinuse &= ~(1U << devno);
    }
    memset(audio_class, 0, sizeof(struct usbh_audio));
}

int usbh_audio_open(struct usbh_audio *audio_class, const char *name, uint32_t samp_freq, uint8_t bitresolution)
{
    struct usb_setup_packet setup;
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t mult;
    uint16_t mps;
    int ret;
    uint8_t intf = 0xff;
    uint8_t altsetting = 1;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }

    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        if (strcmp(name, audio_class->as_msg_table[i].stream_name) == 0) {
            intf = audio_class->as_msg_table[i].stream_intf;
            for (uint8_t j = 1; j < audio_class->as_msg_table[i].num_of_altsetting; j++) {
                if (audio_class->as_msg_table[i].as_format[j].bBitResolution == bitresolution) {
                    for (uint8_t k = 0; k < audio_class->as_msg_table[i].as_format[j].bSamFreqType; k++) {
                        uint32_t freq = 0;

                        memcpy(&freq, &audio_class->as_msg_table[i].as_format[j].tSamFreq[3 * k], 3);
                        if (freq == samp_freq) {
                            altsetting = j;
                            goto freq_found;
                        }
                    }
                }
            }
        }
    }
    return -USB_ERR_NODEV;

freq_found:

    setup.bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = USB_REQUEST_SET_INTERFACE;
    setup.wValue = altsetting;
    setup.wIndex = intf;
    setup.wLength = 0;

    ret = usbh_control_transfer(audio_class->hport, &setup, NULL);
    if (ret < 0) {
        return ret;
    }

    ep_desc = &audio_class->hport->config.intf[intf].altsetting[altsetting].ep[0].ep_desc;

    if (audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].ep_attr & AUDIO_EP_CONTROL_SAMPLING_FEQ) {
        setup.bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_ENDPOINT;
        setup.bRequest = AUDIO_REQUEST_SET_CUR;
        setup.wValue = (AUDIO_EP_CONTROL_SAMPLING_FEQ << 8) | 0x00;
        setup.wIndex = ep_desc->bEndpointAddress;
        setup.wLength = 3;

        memcpy(g_audio_buf, &samp_freq, 3);
        ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
        if (ret < 0) {
            return ret;
        }
    }

    mult = (ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_MASK) >> USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_SHIFT;
    mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    if (ep_desc->bEndpointAddress & 0x80) {
        audio_class->isoin_mps = mps * (mult + 1);
        USBH_EP_INIT(audio_class->isoin, ep_desc);
    } else {
        audio_class->isoout_mps = mps * (mult + 1);
        USBH_EP_INIT(audio_class->isoout, ep_desc);
    }

    USB_LOG_INFO("Open audio stream :%s, altsetting: %u\r\n", name, altsetting);
    audio_class->is_opened = true;
    return ret;
}

int usbh_audio_close(struct usbh_audio *audio_class, const char *name)
{
    struct usb_setup_packet setup;
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint8_t intf = 0xff;
    uint8_t altsetting = 1;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }

    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        if (strcmp(name, audio_class->as_msg_table[i].stream_name) == 0) {
            intf = audio_class->as_msg_table[i].stream_intf;
        }
    }

    if (intf == 0xff) {
        return -USB_ERR_NODEV;
    }

    setup.bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = USB_REQUEST_SET_INTERFACE;
    setup.wValue = 0;
    setup.wIndex = intf;
    setup.wLength = 0;

    ret = usbh_control_transfer(audio_class->hport, &setup, NULL);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("Close audio stream :%s\r\n", name);
    audio_class->is_opened = false;

    ep_desc = &audio_class->hport->config.intf[intf].altsetting[altsetting].ep[0].ep_desc;
    if (ep_desc->bEndpointAddress & 0x80) {
        if (audio_class->isoin) {
            audio_class->isoin = NULL;
        }
    } else {
        if (audio_class->isoout) {
            audio_class->isoout = NULL;
        }
    }

    return ret;
}

int usbh_audio_set_volume(struct usbh_audio *audio_class, const char *name, uint8_t ch, int volume_db)
{
    struct usb_setup_packet setup;
    int ret;
    uint8_t feature_id = 0xff;
    uint8_t intf;
    uint16_t volume_hex;
    int volume_min_db;
    int volume_max_db;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }

    if ((volume_db > 127) || (volume_db < -127)) {
        return -USB_ERR_INVAL;
    }


    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        if (strcmp(name, audio_class->as_msg_table[i].stream_name) == 0) {
            feature_id = audio_class->as_msg_table[i].feature_terminal_id;
            intf = audio_class->as_msg_table[i].stream_intf;
        }
    }

    if (feature_id == 0xff) {
        return -USB_ERR_NODEV;
    }

    setup.bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = AUDIO_REQUEST_GET_CUR;
    setup.wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
    setup.wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup.wLength = 2;

    ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }

    memcpy(&audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_cur, g_audio_buf, 2);

    setup.bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = AUDIO_REQUEST_GET_MIN;
    setup.wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
    setup.wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup.wLength = 2;

    ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }

    memcpy(&audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_min, g_audio_buf, 2);

    setup.bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = AUDIO_REQUEST_GET_MAX;
    setup.wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
    setup.wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup.wLength = 2;

    ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(&audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_max, g_audio_buf, 2);

    setup.bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = AUDIO_REQUEST_GET_RES;
    setup.wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
    setup.wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup.wLength = 2;

    ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(&audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_res, g_audio_buf, 2);

    setup.bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = AUDIO_REQUEST_SET_CUR;
    setup.wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
    setup.wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup.wLength = 2;

    if (audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_min < 0x8000) {
        volume_min_db = audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_min / 256;
    } else {
        volume_min_db = (audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_min - 0x10000) / 256;
    }

    if (audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_max < 0x8000) {
        volume_max_db = audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_max / 256;
    } else {
        volume_max_db = (audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_max - 0x10000) / 256;
    }

    USB_LOG_INFO("Get ch:%u dB range: %ddB ~ %ddB\r\n", ch, volume_min_db, volume_max_db);

    if (volume_db >= 0) {
        volume_hex = volume_db * 256;
        if (volume_hex > audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_max) {
            return -USB_ERR_RANGE;
        }
    } else {
        volume_hex = volume_db * 256 + 0x10000;
        if (volume_hex < audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_min) {
            return -USB_ERR_RANGE;
        }
    }

    memcpy(g_audio_buf, &volume_hex, 2);
    ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }
    audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].volume_cur = volume_hex;
    return ret;
}

int usbh_audio_set_mute(struct usbh_audio *audio_class, const char *name, uint8_t ch, bool mute)
{
    struct usb_setup_packet setup;
    int ret;
    uint8_t feature_id = 0xff;
    uint8_t intf = 0xff;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }

    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        if (strcmp(name, audio_class->as_msg_table[i].stream_name) == 0) {
            feature_id = audio_class->as_msg_table[i].feature_terminal_id;
            intf = audio_class->as_msg_table[i].stream_intf;
        }
    }

    if (feature_id == 0xff) {
        return -USB_ERR_NODEV;
    }

    setup.bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = AUDIO_REQUEST_SET_CUR;
    setup.wValue = (AUDIO_FU_CONTROL_MUTE << 8) | ch;
    setup.wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup.wLength = 1;

    memcpy(g_audio_buf, &mute, 1);
    ret = usbh_control_transfer(audio_class->hport, &setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }
    audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].mute = mute;
    return ret;
}

static void usbh_audio_isoin_callback(void *arg, int nbytes)
{
    struct usbh_audio *audio_class = (struct usbh_audio *)arg;
    struct usbh_urb *urb = audio_class->isoin_urb;

    if (nbytes < 0) {
        USB_LOG_ERR("ISO IN URB error: %d\r\n", nbytes);
        return;
    }

    for (uint32_t i = 0; i < urb->num_of_iso_packets; i++) {
        struct usbh_iso_frame_packet *pkt = &urb->iso_packet[i];
        if (pkt->actual_length > 0 && pkt->errorcode == 0 && audio_class->data_cb) {
            audio_class->data_cb(audio_class, pkt->transfer_buffer, pkt->actual_length, audio_class->data_cb_arg);
        }
    }

    if (audio_class->streaming_in) {
        /* Reset iso_packet for next round */
        for (uint32_t i = 0; i < urb->num_of_iso_packets; i++) {
            urb->iso_packet[i].actual_length = 0;
            urb->iso_packet[i].errorcode = 0;
        }
        urb->errorcode = 0;
        int ret = usbh_submit_urb(urb);
        if (ret < 0) {
            USB_LOG_ERR("ISO IN resubmit failed: %d\r\n", ret);
        }
    }
}

int usbh_audio_start_streaming(struct usbh_audio *audio_class,
                               usbh_audio_data_callback_t cb, void *arg)
{
    struct usbh_urb *urb;
    uint8_t *data_buf;
    uint32_t num_packets = CONFIG_USBH_AUDIO_ISO_PACKETS_PER_URB;
    uint16_t mps;
    uint32_t buf_size;
    int ret;

    if (!audio_class || !audio_class->isoin) {
        return -USB_ERR_INVAL;
    }

    if (audio_class->streaming_in) {
        return -USB_ERR_BUSY;
    }

    mps = audio_class->isoin_mps;
    buf_size = mps * num_packets;

    /* Allocate URB with iso_packet array */
    urb = usb_osal_malloc(sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet) * num_packets);
    if (!urb) {
        return -USB_ERR_NOMEM;
    }
    memset(urb, 0, sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet) * num_packets);

    /* Allocate data buffer */
    data_buf = usb_osal_malloc(buf_size);
    if (!data_buf) {
        usb_osal_free(urb);
        return -USB_ERR_NOMEM;
    }
    memset(data_buf, 0, buf_size);

    /* Configure URB */
    urb->hport = audio_class->hport;
    urb->ep = audio_class->isoin;
    urb->transfer_buffer = data_buf;
    urb->transfer_buffer_length = buf_size;
    urb->timeout = 0;
    urb->complete = usbh_audio_isoin_callback;
    urb->arg = audio_class;
    urb->num_of_iso_packets = num_packets;

    /* Configure each iso_packet */
    for (uint32_t i = 0; i < num_packets; i++) {
        urb->iso_packet[i].transfer_buffer = data_buf + i * mps;
        urb->iso_packet[i].transfer_buffer_length = mps;
        urb->iso_packet[i].actual_length = 0;
        urb->iso_packet[i].errorcode = 0;
    }

    audio_class->isoin_urb = urb;
    audio_class->isoin_data_buf = data_buf;
    audio_class->data_cb = cb;
    audio_class->data_cb_arg = arg;
    audio_class->streaming_in = true;

    ret = usbh_submit_urb(urb);
    if (ret < 0) {
        audio_class->streaming_in = false;
        usb_osal_free(data_buf);
        usb_osal_free(urb);
        audio_class->isoin_urb = NULL;
        audio_class->isoin_data_buf = NULL;
        USB_LOG_ERR("ISO IN submit failed: %d\r\n", ret);
        return ret;
    }

    USB_LOG_INFO("Audio ISO streaming started (mps=%u, packets=%lu)\r\n", mps, (unsigned long)num_packets);
    return 0;
}

/* ---- ISO OUT (playback) ---- */

static void usbh_audio_isoout_callback(void *arg, int nbytes)
{
    struct usbh_audio *audio_class = (struct usbh_audio *)arg;
    struct usbh_urb *urb = audio_class->isoout_urb;

    if (nbytes < 0) {
        USB_LOG_ERR("ISO OUT URB error: %d\r\n", nbytes);
        return;
    }

    if (audio_class->streaming_out && audio_class->playback_cb) {
        /* Ask app to fill each packet buffer for the next round */
        for (uint32_t i = 0; i < urb->num_of_iso_packets; i++) {
            struct usbh_iso_frame_packet *pkt = &urb->iso_packet[i];
            audio_class->playback_cb(audio_class, pkt->transfer_buffer,
                                     pkt->transfer_buffer_length, audio_class->playback_cb_arg);
            pkt->actual_length = 0;
            pkt->errorcode = 0;
        }
        urb->errorcode = 0;
        int ret = usbh_submit_urb(urb);
        if (ret < 0) {
            USB_LOG_ERR("ISO OUT resubmit failed: %d\r\n", ret);
        }
    }
}

int usbh_audio_start_playback(struct usbh_audio *audio_class,
                               usbh_audio_playback_callback_t cb, void *arg)
{
    struct usbh_urb *urb;
    uint8_t *data_buf;
    uint32_t num_packets = CONFIG_USBH_AUDIO_ISO_PACKETS_PER_URB;
    uint16_t mps;
    uint32_t buf_size;
    int ret;

    if (!audio_class || !audio_class->isoout) {
        return -USB_ERR_INVAL;
    }

    if (audio_class->streaming_out) {
        return -USB_ERR_BUSY;
    }

    mps = audio_class->isoout_mps;
    buf_size = mps * num_packets;

    urb = usb_osal_malloc(sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet) * num_packets);
    if (!urb) {
        return -USB_ERR_NOMEM;
    }
    memset(urb, 0, sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet) * num_packets);

    data_buf = usb_osal_malloc(buf_size);
    if (!data_buf) {
        usb_osal_free(urb);
        return -USB_ERR_NOMEM;
    }
    memset(data_buf, 0, buf_size);

    urb->hport = audio_class->hport;
    urb->ep = audio_class->isoout;
    urb->transfer_buffer = data_buf;
    urb->transfer_buffer_length = buf_size;
    urb->timeout = 0;
    urb->complete = usbh_audio_isoout_callback;
    urb->arg = audio_class;
    urb->num_of_iso_packets = num_packets;

    for (uint32_t i = 0; i < num_packets; i++) {
        urb->iso_packet[i].transfer_buffer = data_buf + i * mps;
        urb->iso_packet[i].transfer_buffer_length = mps;
    }

    audio_class->isoout_urb = urb;
    audio_class->isoout_data_buf = data_buf;
    audio_class->playback_cb = cb;
    audio_class->playback_cb_arg = arg;
    audio_class->streaming_out = true;

    /* Fill the first round of data before submitting */
    for (uint32_t i = 0; i < num_packets; i++) {
        cb(audio_class, urb->iso_packet[i].transfer_buffer,
           urb->iso_packet[i].transfer_buffer_length, arg);
    }

    ret = usbh_submit_urb(urb);
    if (ret < 0) {
        audio_class->streaming_out = false;
        usb_osal_free(data_buf);
        usb_osal_free(urb);
        audio_class->isoout_urb = NULL;
        audio_class->isoout_data_buf = NULL;
        USB_LOG_ERR("ISO OUT submit failed: %d\r\n", ret);
        return ret;
    }

    USB_LOG_INFO("Audio ISO playback started (mps=%u, packets=%lu)\r\n", mps, (unsigned long)num_packets);
    return 0;
}

int usbh_audio_stop_streaming(struct usbh_audio *audio_class)
{
    if (!audio_class) {
        return -USB_ERR_INVAL;
    }

    /* Stop ISO IN (recording) */
    if (audio_class->streaming_in) {
        audio_class->streaming_in = false;

        if (audio_class->isoin_urb) {
            usbh_kill_urb(audio_class->isoin_urb);
            usb_osal_free(audio_class->isoin_urb);
            audio_class->isoin_urb = NULL;
        }
        if (audio_class->isoin_data_buf) {
            usb_osal_free(audio_class->isoin_data_buf);
            audio_class->isoin_data_buf = NULL;
        }
        audio_class->data_cb = NULL;
        audio_class->data_cb_arg = NULL;
        USB_LOG_INFO("Audio ISO recording stopped\r\n");
    }

    /* Stop ISO OUT (playback) */
    if (audio_class->streaming_out) {
        audio_class->streaming_out = false;

        if (audio_class->isoout_urb) {
            usbh_kill_urb(audio_class->isoout_urb);
            usb_osal_free(audio_class->isoout_urb);
            audio_class->isoout_urb = NULL;
        }
        if (audio_class->isoout_data_buf) {
            usb_osal_free(audio_class->isoout_data_buf);
            audio_class->isoout_data_buf = NULL;
        }
        audio_class->playback_cb = NULL;
        audio_class->playback_cb_arg = NULL;
        USB_LOG_INFO("Audio ISO playback stopped\r\n");
    }

    return 0;
}

void usbh_audio_list_module(struct usbh_audio *audio_class)
{
    USB_LOG_INFO("============= Audio module information ===================\r\n");
    USB_LOG_RAW("bcdADC :%04x\r\n", audio_class->bcdADC);
    USB_LOG_RAW("Num of audio stream :%u\r\n", audio_class->stream_intf_num);

    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        USB_LOG_RAW("\tstream name :%s\r\n", audio_class->as_msg_table[i].stream_name);
        USB_LOG_RAW("\tstream intf :%u\r\n", audio_class->as_msg_table[i].stream_intf);
        USB_LOG_RAW("\tNum of altsetting :%u\r\n", audio_class->as_msg_table[i].num_of_altsetting);

        for (uint8_t j = 0; j < audio_class->as_msg_table[i].num_of_altsetting; j++) {
            if (j == 0) {
                USB_LOG_RAW("\t\tIngore altsetting 0\r\n");
                continue;
            }
            USB_LOG_RAW("\t\tAltsetting :%u\r\n", j);
            USB_LOG_RAW("\t\t\tbNrChannels :%u\r\n", audio_class->as_msg_table[i].as_format[j].bNrChannels);
            USB_LOG_RAW("\t\t\tbBitResolution :%u\r\n", audio_class->as_msg_table[i].as_format[j].bBitResolution);
            USB_LOG_RAW("\t\t\tbSamFreqType :%u\r\n", audio_class->as_msg_table[i].as_format[j].bSamFreqType);

            for (uint8_t k = 0; k < audio_class->as_msg_table[i].as_format[j].bSamFreqType; k++) {
                uint32_t freq = 0;

                memcpy(&freq, &audio_class->as_msg_table[i].as_format[j].tSamFreq[3 * k], 3);
                USB_LOG_RAW("\t\t\t\tSampleFreq :%u\r\n", freq);
            }
        }
    }

    USB_LOG_INFO("============= Audio module information ===================\r\n");
}

/* ------------------------------------------------------------------ */
/*  AC unit graph — lightweight topology for complex audio devices      */
/* ------------------------------------------------------------------ */
#define AC_MAX_UNITS 16 /* enough for most USB audio devices */

struct ac_unit_entry {
    uint8_t id;
    uint8_t subtype;
    uint8_t source_id;     /* first / only bSourceID (IT/OT/FU/SU) */
    uint16_t terminal_type; /* valid for IT/OT only */
};

/*
 * Walk backward through the AC unit graph from |start_id| following
 * bSourceID links.  Return the first unit of the requested |subtype|
 * found in the chain, or NULL if not found (max depth 8).
 */
static const struct ac_unit_entry *ac_find_unit_in_chain(
    const struct ac_unit_entry *units, uint8_t num_units,
    uint8_t start_id, uint8_t target_subtype)
{
    uint8_t cur_id = start_id;

    for (uint8_t depth = 0; depth < 8; depth++) {
        const struct ac_unit_entry *u = NULL;

        for (uint8_t j = 0; j < num_units; j++) {
            if (units[j].id == cur_id) {
                u = &units[j];
                break;
            }
        }
        if (!u) {
            break;
        }
        if (u->subtype == target_subtype) {
            return u;
        }
        if (u->source_id == 0 || u->source_id == cur_id) {
            break; /* dead end */
        }
        cur_id = u->source_id;
    }
    return NULL;
}

static int usbh_audio_ctrl_connect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret;
    uint8_t cur_iface = 0;
    uint8_t cur_iface_count = 0;
    uint8_t cur_alt_setting = 0;
    uint8_t input_offset = 0;
    uint8_t output_offset = 0;
    uint8_t *p;
    struct usbh_audio_ac_msg ac_msg_table[CONFIG_USBHOST_AUDIO_MAX_STREAMS];

    /* Flat table of all AC units for topology traversal */
    struct ac_unit_entry ac_units[AC_MAX_UNITS];
    uint8_t num_ac_units = 0;

    struct usbh_audio *audio_class = usbh_audio_class_alloc();
    if (audio_class == NULL) {
        USB_LOG_ERR("Fail to alloc audio_class\r\n");
        return -USB_ERR_NOMEM;
    }

    audio_class->hport = hport;
    audio_class->ctrl_intf = intf;
    hport->config.intf[intf].priv = audio_class;

    memset(ac_msg_table, 0, sizeof(ac_msg_table));
    memset(ac_units, 0, sizeof(ac_units));

    /* ---- First pass: parse all descriptors ---- */
    p = hport->raw_config_desc;
    while (p[DESC_bLength]) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
                cur_iface_count = p[3];
                break;
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                cur_iface = p[INTF_DESC_bInterfaceNumber];
                cur_alt_setting = p[INTF_DESC_bAlternateSetting];
                break;
            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                break;
            case AUDIO_INTERFACE_DESCRIPTOR_TYPE:
                if (cur_iface == audio_class->ctrl_intf) {
                    switch (p[DESC_bDescriptorSubType]) {
                        case AUDIO_CONTROL_HEADER: {
                            struct audio_cs_if_ac_header_descriptor *desc = (struct audio_cs_if_ac_header_descriptor *)p;
                            audio_class->bcdADC = desc->bcdADC;
                            audio_class->bInCollection = desc->bInCollection;
                        } break;
                        case AUDIO_CONTROL_INPUT_TERMINAL: {
                            struct audio_cs_if_ac_input_terminal_descriptor *desc = (struct audio_cs_if_ac_input_terminal_descriptor *)p;

                            if (input_offset < CONFIG_USBHOST_AUDIO_MAX_STREAMS) {
                                memcpy(&ac_msg_table[input_offset].ac_input, desc, sizeof(struct audio_cs_if_ac_input_terminal_descriptor));
                                input_offset++;
                            }
                            /* Record in unit graph */
                            if (num_ac_units < AC_MAX_UNITS) {
                                ac_units[num_ac_units].id = desc->bTerminalID;
                                ac_units[num_ac_units].subtype = AUDIO_CONTROL_INPUT_TERMINAL;
                                ac_units[num_ac_units].source_id = 0; /* IT has no source */
                                ac_units[num_ac_units].terminal_type = desc->wTerminalType;
                                num_ac_units++;
                            }
                        } break;
                        case AUDIO_CONTROL_OUTPUT_TERMINAL: {
                            struct audio_cs_if_ac_output_terminal_descriptor *desc = (struct audio_cs_if_ac_output_terminal_descriptor *)p;

                            if (output_offset < CONFIG_USBHOST_AUDIO_MAX_STREAMS) {
                                memcpy(&ac_msg_table[output_offset].ac_output, desc, sizeof(struct audio_cs_if_ac_output_terminal_descriptor));
                                output_offset++;
                            }
                            if (num_ac_units < AC_MAX_UNITS) {
                                ac_units[num_ac_units].id = desc->bTerminalID;
                                ac_units[num_ac_units].subtype = AUDIO_CONTROL_OUTPUT_TERMINAL;
                                ac_units[num_ac_units].source_id = desc->bSourceID;
                                ac_units[num_ac_units].terminal_type = desc->wTerminalType;
                                num_ac_units++;
                            }
                        } break;
                        case AUDIO_CONTROL_FEATURE_UNIT: {
                            struct audio_cs_if_ac_feature_unit_descriptor *desc = (struct audio_cs_if_ac_feature_unit_descriptor *)p;

                            if (num_ac_units < AC_MAX_UNITS) {
                                ac_units[num_ac_units].id = desc->bUnitID;
                                ac_units[num_ac_units].subtype = AUDIO_CONTROL_FEATURE_UNIT;
                                ac_units[num_ac_units].source_id = desc->bSourceID;
                                ac_units[num_ac_units].terminal_type = 0;
                                num_ac_units++;
                            }
                        } break;
                        case AUDIO_CONTROL_MIXER_UNIT: {
                            /* Mixer: bUnitID at p[3], bNrInPins at p[4], first source at p[5] */
                            if (num_ac_units < AC_MAX_UNITS) {
                                ac_units[num_ac_units].id = p[3];
                                ac_units[num_ac_units].subtype = AUDIO_CONTROL_MIXER_UNIT;
                                ac_units[num_ac_units].source_id = p[5]; /* first input pin */
                                ac_units[num_ac_units].terminal_type = 0;
                                num_ac_units++;
                            }
                            USB_LOG_DBG("Mixer Unit %u (source=%u)\r\n", p[3], p[5]);
                        } break;
                        case AUDIO_CONTROL_SELECTOR_UNIT: {
                            /* Selector: bUnitID at p[3], bNrInPins at p[4], first source at p[5] */
                            if (num_ac_units < AC_MAX_UNITS) {
                                ac_units[num_ac_units].id = p[3];
                                ac_units[num_ac_units].subtype = AUDIO_CONTROL_SELECTOR_UNIT;
                                ac_units[num_ac_units].source_id = p[5]; /* first input pin */
                                ac_units[num_ac_units].terminal_type = 0;
                                num_ac_units++;
                            }
                            USB_LOG_DBG("Selector Unit %u (source=%u)\r\n", p[3], p[5]);
                        } break;
                        default:
                            /* Skip unknown AC subtypes (Processing Unit, Extension Unit, etc.) */
                            USB_LOG_WRN("Skipping AC subtype 0x%02x\r\n", p[DESC_bDescriptorSubType]);
                            break;
                    }
                } else if ((cur_iface > audio_class->ctrl_intf) &&
                           (cur_iface < (audio_class->ctrl_intf + cur_iface_count))) {
                    switch (p[DESC_bDescriptorSubType]) {
                        case AUDIO_STREAMING_GENERAL: {
                            struct audio_cs_if_as_general_descriptor *desc = (struct audio_cs_if_as_general_descriptor *)p;

                            /* all altsetting have the same general */
                            audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].stream_intf = cur_iface;
                            memcpy(&audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].as_general, desc, sizeof(struct audio_cs_if_as_general_descriptor));
                        } break;
                        case AUDIO_STREAMING_FORMAT_TYPE: {
                            struct audio_cs_if_as_format_type_descriptor *desc = (struct audio_cs_if_as_format_type_descriptor *)p;
                            if (cur_alt_setting < CONFIG_USBHOST_MAX_INTF_ALTSETTINGS) {
                                audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].num_of_altsetting = (cur_alt_setting + 1);
                                memcpy(&audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].as_format[cur_alt_setting], desc,
                                       MIN(desc->bLength, sizeof(struct audio_cs_if_as_format_type_descriptor)));
                            }
                        } break;
                        default:
                            break;
                    }
                }
                break;
            case AUDIO_ENDPOINT_DESCRIPTOR_TYPE:
                if ((cur_iface > audio_class->ctrl_intf) &&
                    (cur_iface < (audio_class->ctrl_intf + cur_iface_count))) {
                    if (p[DESC_bDescriptorSubType] == AUDIO_ENDPOINT_GENERAL) {
                        struct audio_cs_ep_ep_general_descriptor *desc = (struct audio_cs_ep_ep_general_descriptor *)p;
                        audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].ep_attr = desc->bmAttributes;
                    }
                }
                break;
            default:
                break;
        }
        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

    /*
     * If no IAD was found (cur_iface_count == 0), fall back to bInCollection
     * from the AC header to determine how many streaming interfaces follow.
     * Many USB 1.x audio devices omit the IAD.
     */
    if (cur_iface_count == 0 && audio_class->bInCollection > 0) {
        USB_LOG_WRN("No IAD found, using bInCollection=%u\r\n", audio_class->bInCollection);
        cur_iface_count = audio_class->bInCollection + 1; /* +1 for AC intf itself */

        /* Re-parse streaming interface descriptors with corrected range */
        p = hport->raw_config_desc;
        cur_iface = 0;
        cur_alt_setting = 0;
        while (p[DESC_bLength]) {
            switch (p[DESC_bDescriptorType]) {
                case USB_DESCRIPTOR_TYPE_INTERFACE:
                    cur_iface = p[INTF_DESC_bInterfaceNumber];
                    cur_alt_setting = p[INTF_DESC_bAlternateSetting];
                    break;
                case AUDIO_INTERFACE_DESCRIPTOR_TYPE:
                    if ((cur_iface > audio_class->ctrl_intf) &&
                        (cur_iface <= (audio_class->ctrl_intf + audio_class->bInCollection))) {
                        uint8_t sidx = cur_iface - audio_class->ctrl_intf - 1;
                        if (sidx < CONFIG_USBHOST_AUDIO_MAX_STREAMS) {
                            switch (p[DESC_bDescriptorSubType]) {
                                case AUDIO_STREAMING_GENERAL: {
                                    struct audio_cs_if_as_general_descriptor *desc = (struct audio_cs_if_as_general_descriptor *)p;
                                    audio_class->as_msg_table[sidx].stream_intf = cur_iface;
                                    memcpy(&audio_class->as_msg_table[sidx].as_general, desc, sizeof(struct audio_cs_if_as_general_descriptor));
                                } break;
                                case AUDIO_STREAMING_FORMAT_TYPE: {
                                    struct audio_cs_if_as_format_type_descriptor *desc = (struct audio_cs_if_as_format_type_descriptor *)p;
                                    if (cur_alt_setting < CONFIG_USBHOST_MAX_INTF_ALTSETTINGS) {
                                        audio_class->as_msg_table[sidx].num_of_altsetting = (cur_alt_setting + 1);
                                        memcpy(&audio_class->as_msg_table[sidx].as_format[cur_alt_setting], desc,
                                               MIN(desc->bLength, sizeof(struct audio_cs_if_as_format_type_descriptor)));
                                    }
                                } break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
                case AUDIO_ENDPOINT_DESCRIPTOR_TYPE:
                    if ((cur_iface > audio_class->ctrl_intf) &&
                        (cur_iface <= (audio_class->ctrl_intf + audio_class->bInCollection))) {
                        uint8_t sidx = cur_iface - audio_class->ctrl_intf - 1;
                        if (sidx < CONFIG_USBHOST_AUDIO_MAX_STREAMS && p[DESC_bDescriptorSubType] == AUDIO_ENDPOINT_GENERAL) {
                            struct audio_cs_ep_ep_general_descriptor *desc = (struct audio_cs_ep_ep_general_descriptor *)p;
                            audio_class->as_msg_table[sidx].ep_attr = desc->bmAttributes;
                        }
                    }
                    break;
                default:
                    break;
            }
            p += p[DESC_bLength];
        }
    }

    /* Use bInCollection as stream count (more reliable than input_offset for complex topologies) */
    audio_class->stream_intf_num = audio_class->bInCollection;
    if (audio_class->stream_intf_num > CONFIG_USBHOST_AUDIO_MAX_STREAMS) {
        audio_class->stream_intf_num = CONFIG_USBHOST_AUDIO_MAX_STREAMS;
    }

    USB_LOG_INFO("AC units: %u IT, %u OT, %u total units, %u streams\r\n",
                 input_offset, output_offset, num_ac_units, audio_class->stream_intf_num);

    /* ---- Resolve stream names & feature units via topology graph ---- */
    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        uint8_t link_id = audio_class->as_msg_table[i].as_general.bTerminalLink;

        /* Find the linked terminal in the IT/OT tables */
        for (uint8_t j = 0; j < input_offset; j++) {
            if (ac_msg_table[j].ac_input.bTerminalID == link_id &&
                ac_msg_table[j].ac_input.wTerminalType == 0x0101) {
                /*
                 * bTerminalLink → Input Terminal (USB Streaming)
                 * This is a PLAYBACK stream: USB Host → device → physical output
                 * Walk the unit graph to find the physical Output Terminal.
                 */
                audio_class->as_msg_table[i].input_terminal_id = link_id;

                /* Find the OT that is NOT USB Streaming — that's our physical output */
                for (uint8_t k = 0; k < output_offset; k++) {
                    if (ac_msg_table[k].ac_output.wTerminalType != 0x0101) {
                        audio_class->as_msg_table[i].output_terminal_id = ac_msg_table[k].ac_output.bTerminalID;

                        switch (ac_msg_table[k].ac_output.wTerminalType) {
                            case AUDIO_OUTTERM_SPEAKER:
                                audio_class->as_msg_table[i].stream_name = "speaker";
                                break;
                            case AUDIO_OUTTERM_HEADPHONES:
                                audio_class->as_msg_table[i].stream_name = "headphones";
                                break;
                            case AUDIO_OUTTERM_HEADDISPLAY:
                                audio_class->as_msg_table[i].stream_name = "headdisplay";
                                break;
                            default:
                                audio_class->as_msg_table[i].stream_name = "speaker";
                                break;
                        }

                        /* Walk backward from OT to find Feature Unit in the chain */
                        const struct ac_unit_entry *fu = ac_find_unit_in_chain(
                            ac_units, num_ac_units,
                            ac_msg_table[k].ac_output.bSourceID,
                            AUDIO_CONTROL_FEATURE_UNIT);
                        if (fu) {
                            audio_class->as_msg_table[i].feature_terminal_id = fu->id;
                        }
                        break;
                    }
                }
                break;
            }
        }

        /* If not found as IT link, check OT link */
        if (audio_class->as_msg_table[i].stream_name == NULL) {
            for (uint8_t j = 0; j < output_offset; j++) {
                if (ac_msg_table[j].ac_output.bTerminalID == link_id &&
                    ac_msg_table[j].ac_output.wTerminalType == 0x0101) {
                    /*
                     * bTerminalLink → Output Terminal (USB Streaming)
                     * This is a CAPTURE stream: physical input → device → USB Host
                     * Walk backward from OT through the chain to find Feature Unit and IT.
                     */
                    audio_class->as_msg_table[i].output_terminal_id = link_id;

                    /* Walk from OT source backward to find Feature Unit */
                    const struct ac_unit_entry *fu = ac_find_unit_in_chain(
                        ac_units, num_ac_units,
                        ac_msg_table[j].ac_output.bSourceID,
                        AUDIO_CONTROL_FEATURE_UNIT);
                    if (fu) {
                        audio_class->as_msg_table[i].feature_terminal_id = fu->id;
                    }

                    /* Walk further to find the physical Input Terminal */
                    const struct ac_unit_entry *it = ac_find_unit_in_chain(
                        ac_units, num_ac_units,
                        ac_msg_table[j].ac_output.bSourceID,
                        AUDIO_CONTROL_INPUT_TERMINAL);
                    if (it) {
                        audio_class->as_msg_table[i].input_terminal_id = it->id;

                        switch (it->terminal_type) {
                            case AUDIO_INTERM_MIC:
                            case AUDIO_INTERM_DESKTOP_MIC:
                            case AUDIO_INTERM_PERSONAL_MIC:
                            case AUDIO_INTERM_OMNI_MIC:
                            case AUDIO_INTERM_MIC_ARRAY:
                            case AUDIO_INTERM_PROC_MIC_ARRAY:
                                audio_class->as_msg_table[i].stream_name = "mic";
                                break;
                            default:
                                audio_class->as_msg_table[i].stream_name = "mic";
                                break;
                        }
                    } else {
                        audio_class->as_msg_table[i].stream_name = "mic";
                    }
                    break;
                }
            }
        }
    }

    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        if (audio_class->as_msg_table[i].stream_name == NULL) {
            USB_LOG_WRN("Stream %u: could not resolve name, default to 'unknown'\r\n", i);
            audio_class->as_msg_table[i].stream_name = "unknown";
        }
    }

    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        ret = usbh_audio_close(audio_class, audio_class->as_msg_table[i].stream_name);
        if (ret < 0) {
            USB_LOG_ERR("Fail to close audio stream :%s\r\n", audio_class->as_msg_table[i].stream_name);
            return ret;
        }
    }

    usbh_audio_list_module(audio_class);

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, audio_class->minor);
    USB_LOG_INFO("Register Audio Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_audio_run(audio_class);
    return 0;
}

static int usbh_audio_ctrl_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_audio *audio_class = (struct usbh_audio *)hport->config.intf[intf].priv;

    if (audio_class) {
        if (audio_class->isoin) {
        }

        if (audio_class->isoout) {
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            usb_osal_thread_schedule_other();
            USB_LOG_INFO("Unregister Audio Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_audio_stop(audio_class);
        }

        usbh_audio_class_free(audio_class);
    }

    return ret;
}

static int usbh_audio_data_connect(struct usbh_hubport *hport, uint8_t intf)
{
    (void)hport;
    (void)intf;
    return 0;
}

static int usbh_audio_data_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    (void)hport;
    (void)intf;
    return 0;
}

__WEAK void usbh_audio_run(struct usbh_audio *audio_class)
{
    (void)audio_class;
}

__WEAK void usbh_audio_stop(struct usbh_audio *audio_class)
{
    (void)audio_class;
}

const struct usbh_class_driver audio_ctrl_class_driver = {
    .driver_name = "audio_ctrl",
    .connect = usbh_audio_ctrl_connect,
    .disconnect = usbh_audio_ctrl_disconnect
};

const struct usbh_class_driver audio_streaming_class_driver = {
    .driver_name = "audio_streaming",
    .connect = usbh_audio_data_connect,
    .disconnect = usbh_audio_data_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info audio_ctrl_intf_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS,
    .bInterfaceClass = USB_DEVICE_CLASS_AUDIO,
    .bInterfaceSubClass = AUDIO_SUBCLASS_AUDIOCONTROL,
    .bInterfaceProtocol = 0x00,
    .id_table = NULL,
    .class_driver = &audio_ctrl_class_driver
};

CLASS_INFO_DEFINE const struct usbh_class_info audio_streaming_intf_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS,
    .bInterfaceClass = USB_DEVICE_CLASS_AUDIO,
    .bInterfaceSubClass = AUDIO_SUBCLASS_AUDIOSTREAMING,
    .bInterfaceProtocol = 0x00,
    .id_table = NULL,
    .class_driver = &audio_streaming_class_driver
};
