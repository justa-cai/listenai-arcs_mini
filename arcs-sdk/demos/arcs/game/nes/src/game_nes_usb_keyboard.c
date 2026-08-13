/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "game_nes_usb_keyboard.h"

#ifdef CONFIG_GAME_NES_USB_HOST_KEYBOARD

#include <stdbool.h>
#include <stdint.h>

#include "ClockManager.h"
#include "FreeRTOS.h"
#include "arcs_ap.h"
#include "game_nes_input.h"
#include "lisa_log.h"
#include "sys_init.h"
#include "task.h"
#include "usb_hid.h"
#include "usbh_core.h"
#include "usbh_hid.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "game_nes_usb"

#define GAME_NES_USB_BUSID     0
#define GAME_NES_USB_REG_BASE  0x41000000UL
#define GAME_NES_HID_KEY(ch)   ((uint8_t)(HID_KBD_USAGE_A + ((ch) - 'A')))

static volatile uint16_t g_usb_joypad_state;
static volatile bool g_usb_keyboard_connected;
static struct usbh_hid *g_usb_keyboard_hid;
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_usb_keyboard_report[64];

static void game_nes_usb_keyboard_callback(void *arg, int nbytes);

static bool game_nes_usb_hid_is_keyboard(const struct usbh_hid *hid_class)
{
    if (!hid_class || !hid_class->hport) {
        return false;
    }

    const struct usb_interface_descriptor *desc =
        &hid_class->hport->config.intf[hid_class->intf].altsetting[0].intf_desc;
    return desc->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD;
}

static uint16_t game_nes_usb_key_to_btn(uint8_t key)
{
    switch (key) {
    case HID_KBD_USAGE_UP:
    case HID_KBD_USAGE_KPDUP:
    case GAME_NES_HID_KEY('W'):
        return GAME_NES_BTN_U1;
    case HID_KBD_USAGE_DOWN:
    case HID_KBD_USAGE_KPDDOWN:
    case GAME_NES_HID_KEY('S'):
        return GAME_NES_BTN_D1;
    case HID_KBD_USAGE_LEFT:
    case HID_KBD_USAGE_KPDLEFT:
    case GAME_NES_HID_KEY('A'):
        return GAME_NES_BTN_L1;
    case HID_KBD_USAGE_RIGHT:
    case HID_KBD_USAGE_KPDRIGHT:
    case GAME_NES_HID_KEY('D'):
        return GAME_NES_BTN_R1;
    case GAME_NES_HID_KEY('X'):
    case GAME_NES_HID_KEY('K'):
        return GAME_NES_BTN_A1;
    case GAME_NES_HID_KEY('Z'):
    case GAME_NES_HID_KEY('J'):
        return GAME_NES_BTN_B1;
    case HID_KBD_USAGE_ENTER:
    case HID_KBD_USAGE_KPDEMTER:
        return GAME_NES_BTN_ST1;
    case HID_KBD_USAGE_SPACE:
    case HID_KBD_USAGE_TAB:
        return GAME_NES_BTN_SE1;
    default:
        return 0;
    }
}

static uint16_t game_nes_usb_report_to_joypad(const struct usb_hid_kbd_report *report)
{
    uint16_t state = 0;

    if (!report) {
        return 0;
    }

    if (report->modifier & (HID_MODIFIER_LCTRL | HID_MODIFIER_RCTRL)) {
        state |= GAME_NES_BTN_B1;
    }
    if (report->modifier & (HID_MODIFIER_LALT | HID_MODIFIER_RALT)) {
        state |= GAME_NES_BTN_A1;
    }

    for (uint8_t i = 0; i < 6U; i++) {
        state |= game_nes_usb_key_to_btn(report->key[i]);
    }

    return state;
}

static void game_nes_usb_submit_keyboard_urb(struct usbh_hid *hid_class)
{
    if (!hid_class || !hid_class->intin || hid_class != g_usb_keyboard_hid) {
        return;
    }

    uint32_t packet_size = hid_class->intin->wMaxPacketSize;
    if (packet_size > sizeof(g_usb_keyboard_report)) {
        packet_size = sizeof(g_usb_keyboard_report);
    }

    usbh_int_urb_fill(&hid_class->intin_urb,
                      hid_class->hport,
                      hid_class->intin,
                      g_usb_keyboard_report,
                      packet_size,
                      0,
                      game_nes_usb_keyboard_callback,
                      hid_class);
    (void)usbh_submit_urb(&hid_class->intin_urb);
}

static void game_nes_usb_keyboard_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (hid_class != g_usb_keyboard_hid) {
        return;
    }

    if (nbytes >= (int)sizeof(struct usb_hid_kbd_report)) {
        const struct usb_hid_kbd_report *report =
            (const struct usb_hid_kbd_report *)g_usb_keyboard_report;
        g_usb_joypad_state = game_nes_usb_report_to_joypad(report);
        game_nes_usb_submit_keyboard_urb(hid_class);
    } else if (nbytes > 0) {
        g_usb_joypad_state = 0;
        game_nes_usb_submit_keyboard_urb(hid_class);
    } else if (nbytes == -USB_ERR_NAK) {
        game_nes_usb_submit_keyboard_urb(hid_class);
    } else {
        g_usb_joypad_state = 0;
        LISA_LOGW(LOG_TAG, "USB keyboard interrupt stopped: %d", nbytes);
    }
}

uint16_t game_nes_usb_keyboard_get_state(void)
{
    return g_usb_keyboard_connected ? g_usb_joypad_state : 0;
}

void usbh_hid_run(struct usbh_hid *hid_class)
{
    if (!hid_class || !hid_class->hport) {
        return;
    }

    if (!game_nes_usb_hid_is_keyboard(hid_class)) {
        const struct usb_interface_descriptor *desc =
            &hid_class->hport->config.intf[hid_class->intf].altsetting[0].intf_desc;
        LISA_LOGI(LOG_TAG, "Ignore HID interface protocol=%u", desc->bInterfaceProtocol);
        return;
    }

    if (g_usb_keyboard_hid && g_usb_keyboard_hid != hid_class) {
        LISA_LOGI(LOG_TAG, "Ignore additional USB keyboard intf=%u", hid_class->intf);
        return;
    }

    (void)usbh_hid_set_protocol(hid_class, 0);
    (void)usbh_hid_set_idle(hid_class, 0, 0);
    g_usb_keyboard_hid = hid_class;
    g_usb_joypad_state = 0;
    g_usb_keyboard_connected = true;

    LISA_LOGI(LOG_TAG, "USB keyboard connected: bus=%u hub=%u port=%u intf=%u",
              hid_class->hport->bus->busid,
              hid_class->hport->parent ? hid_class->hport->parent->index : 0,
              hid_class->hport->port,
              hid_class->intf);
    game_nes_usb_submit_keyboard_urb(hid_class);
}

void usbh_hid_stop(struct usbh_hid *hid_class)
{
    if (!game_nes_usb_hid_is_keyboard(hid_class) || hid_class != g_usb_keyboard_hid) {
        return;
    }

    g_usb_keyboard_hid = NULL;
    g_usb_joypad_state = 0;
    g_usb_keyboard_connected = false;
    LISA_LOGI(LOG_TAG, "USB keyboard disconnected");
}

static void game_nes_usb_event_handler(uint8_t busid,
                                       uint8_t hub_index,
                                       uint8_t hub_port,
                                       uint8_t intf,
                                       uint8_t event)
{
    const char *name = "UNKNOWN";

    switch (event) {
    case USBH_EVENT_ERROR:
        name = "ERROR";
        break;
    case USBH_EVENT_SOF:
        name = "SOF";
        break;
    case USBH_EVENT_DEVICE_RESET:
        name = "DEVICE_RESET";
        break;
    case USBH_EVENT_DEVICE_CONNECTED:
        name = "DEVICE_CONNECTED";
        break;
    case USBH_EVENT_DEVICE_DISCONNECTED:
        name = "DEVICE_DISCONNECTED";
        break;
    case USBH_EVENT_DEVICE_CONFIGURED:
        name = "DEVICE_CONFIGURED";
        break;
    case USBH_EVENT_INTERFACE_START:
        name = "INTERFACE_START";
        break;
    case USBH_EVENT_INTERFACE_STOP:
        name = "INTERFACE_STOP";
        break;
    case USBH_EVENT_INIT:
        name = "INIT";
        break;
    case USBH_EVENT_DEINIT:
        name = "DEINIT";
        break;
    default:
        break;
    }

    LISA_LOGI(LOG_TAG, "USB event: bus=%u hub=%u port=%u intf=%u event=%u(%s)",
              busid, hub_index, hub_port, intf, event, name);
}

void game_nes_usb_keyboard_init(void)
{
    static bool initialized;

    if (initialized) {
        return;
    }
    initialized = true;

    g_usb_joypad_state = 0;
    g_usb_keyboard_connected = false;
    g_usb_keyboard_hid = NULL;
    LISA_LOGI(LOG_TAG, "Initializing USB Host keyboard input");
    usbh_initialize(GAME_NES_USB_BUSID, GAME_NES_USB_REG_BASE, game_nes_usb_event_handler);
}

static int game_nes_usb_host_phy_init(void)
{
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
    return 0;
}

SYS_INIT(game_nes_usb_host_phy_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);

#endif
