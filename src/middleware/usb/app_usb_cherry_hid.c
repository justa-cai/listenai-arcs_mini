/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lisa_log.h"
#include "usbh_core.h"
#include "usbh_hid.h"

#define TAG "usb-hid"

#define APP_USB_HID_REPORT_SIZE             64U
#define APP_USB_HID_BOOT_MOUSE_REPORT_SIZE  4
#define APP_USB_HID_EXT_MOUSE_REPORT_SIZE   7
#define APP_USB_HID_MOUSE_REPORT_ID         0x01U
#define APP_USB_HID_MOUSE_LEFT_BUTTON_MASK  0x01U

USBH_HID_BUFFER_SECTION USB_MEM_ALIGNX
static uint8_t s_hid_report[CONFIG_USBHOST_MAX_HID_CLASS][APP_USB_HID_REPORT_SIZE];
static bool s_hid_active[CONFIG_USBHOST_MAX_HID_CLASS];
static bool s_hid_mouse_left_pressed[CONFIG_USBHOST_MAX_HID_CLASS];

static void app_usb_hid_report_callback(void *arg, int nbytes);

__attribute__((weak)) void app_usb_hid_mouse_left_click_from_isr(void)
{
}

static const char *app_usb_hid_protocol_name(uint8_t protocol)
{
    switch (protocol) {
    case HID_PROTOCOL_KEYBOARD:
        return "keyboard";
    case HID_PROTOCOL_MOUSE:
        return "mouse";
    default:
        return "generic";
    }
}

static void app_usb_hid_submit(struct usbh_hid *hid)
{
    uint8_t minor = hid->minor;
    uint16_t packet_size;
    int ret;

    if (minor >= CONFIG_USBHOST_MAX_HID_CLASS || !s_hid_active[minor] ||
        hid->hport == NULL || !hid->hport->connected || hid->intin == NULL) {
        return;
    }

    packet_size = hid->intin->wMaxPacketSize;
    if (packet_size == 0U || packet_size > APP_USB_HID_REPORT_SIZE) {
        LISA_LOGE(TAG, "/dev/input%u unsupported report size: %u",
                  minor, packet_size);
        s_hid_active[minor] = false;
        return;
    }

    usbh_int_urb_fill(&hid->intin_urb, hid->hport, hid->intin,
                      s_hid_report[minor], packet_size, 0,
                      app_usb_hid_report_callback, hid);
    ret = usbh_submit_urb(&hid->intin_urb);
    if (ret < 0) {
        LISA_LOGE(TAG, "/dev/input%u submit failed: %d", minor, ret);
        s_hid_active[minor] = false;
    }
}

static void app_usb_hid_handle_mouse_report(struct usbh_hid *hid, int nbytes)
{
    uint8_t minor = hid->minor;
    uint8_t buttons;
    bool pressed;

    if (hid->protocol != HID_PROTOCOL_MOUSE) {
        return;
    }

    if (nbytes == APP_USB_HID_BOOT_MOUSE_REPORT_SIZE) {
        buttons = s_hid_report[minor][0];
    } else if (nbytes == APP_USB_HID_EXT_MOUSE_REPORT_SIZE &&
               s_hid_report[minor][0] == APP_USB_HID_MOUSE_REPORT_ID) {
        buttons = s_hid_report[minor][1];
    } else {
        return;
    }

    pressed = (buttons & APP_USB_HID_MOUSE_LEFT_BUTTON_MASK) != 0U;
    if (pressed && !s_hid_mouse_left_pressed[minor]) {
        app_usb_hid_mouse_left_click_from_isr();
    }
    s_hid_mouse_left_pressed[minor] = pressed;
}

static void app_usb_hid_report_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid = (struct usbh_hid *)arg;
    char report[(APP_USB_HID_REPORT_SIZE * 3U) + 1U];
    size_t offset = 0U;
    uint8_t minor;

    if (hid == NULL || hid->minor >= CONFIG_USBHOST_MAX_HID_CLASS) {
        return;
    }

    minor = hid->minor;
    if (!s_hid_active[minor]) {
        return;
    }

    if (nbytes > 0) {
        for (int i = 0; i < nbytes && i < APP_USB_HID_REPORT_SIZE; i++) {
            offset += (size_t)snprintf(&report[offset], sizeof(report) - offset,
                                       "%02X%s", s_hid_report[minor][i],
                                       i + 1 == nbytes ? "" : " ");
        }
        LISA_LOGD(TAG, "/dev/input%u report (%d): %s", minor, nbytes, report);
        app_usb_hid_handle_mouse_report(hid, nbytes);
    } else if (nbytes != -USB_ERR_NAK) {
        LISA_LOGE(TAG, "/dev/input%u transfer failed: %d", minor, nbytes);
        s_hid_active[minor] = false;
        return;
    }

    app_usb_hid_submit(hid);
}

void usbh_hid_run(struct usbh_hid *hid)
{
    uint8_t minor;

    if (hid == NULL || hid->minor >= CONFIG_USBHOST_MAX_HID_CLASS) {
        LISA_LOGE(TAG, "invalid HID instance");
        return;
    }

    minor = hid->minor;
    s_hid_active[minor] = true;
    s_hid_mouse_left_pressed[minor] = false;
    LISA_LOGI(TAG, "/dev/input%u attached: %s, report=%u, packet=%u",
              minor, app_usb_hid_protocol_name(hid->protocol),
              hid->report_size, hid->intin == NULL ? 0U : hid->intin->wMaxPacketSize);
    app_usb_hid_submit(hid);
}

void usbh_hid_stop(struct usbh_hid *hid)
{
    if (hid == NULL || hid->minor >= CONFIG_USBHOST_MAX_HID_CLASS) {
        return;
    }

    s_hid_active[hid->minor] = false;
    s_hid_mouse_left_pressed[hid->minor] = false;
    LISA_LOGI(TAG, "/dev/input%u detached", hid->minor);
}
