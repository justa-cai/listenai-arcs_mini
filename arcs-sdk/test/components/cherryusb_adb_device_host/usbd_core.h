#ifndef TEST_USBD_CORE_H
#define TEST_USBD_CORE_H

#include <stdbool.h>
#include <stdint.h>

#define USB_NOCACHE_RAM_SECTION
#define USB_MEM_ALIGNX
#define USB_ALIGN_UP(size, align) (((size) + ((align) - 1U)) & ~((align) - 1U))

enum usbd_event_type {
    USBD_EVENT_ERROR,
    USBD_EVENT_RESET,
    USBD_EVENT_SOF,
    USBD_EVENT_CONNECTED,
    USBD_EVENT_DISCONNECTED,
    USBD_EVENT_SUSPEND,
    USBD_EVENT_RESUME,
    USBD_EVENT_CONFIGURED,
    USBD_EVENT_SET_INTERFACE,
    USBD_EVENT_SET_REMOTE_WAKEUP,
    USBD_EVENT_CLR_REMOTE_WAKEUP,
    USBD_EVENT_INIT,
    USBD_EVENT_DEINIT,
    USBD_EVENT_UNKNOWN
};

typedef void (*usbd_endpoint_callback)(uint8_t busid, uint8_t ep, uint32_t nbytes);
typedef void (*usbd_notify_handler)(uint8_t busid, uint8_t event, void *arg);

struct usbd_endpoint {
    uint8_t ep_addr;
    usbd_endpoint_callback ep_cb;
};

struct usbd_interface {
    void *class_interface_handler;
    void *class_endpoint_handler;
    void *vendor_handler;
    usbd_notify_handler notify_handler;
};

int usbd_ep_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len);
int usbd_ep_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len);
void usbd_add_endpoint(uint8_t busid, struct usbd_endpoint *ep);

#endif
