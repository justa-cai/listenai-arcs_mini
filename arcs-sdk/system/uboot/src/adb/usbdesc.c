#include "xutils.h"

#include "usb_config.h"
#include "usbd_core.h"
#include "usbd_adb.h"
#include "adb_shell.h"
#include "adb_sync.h"

/*!< endpoint address */
#define WINUSB_IN_EP       0x81
#define WINUSB_OUT_EP      0x02
#define USBD_VID           0x0483
#define USBD_PID           0x0adb

#define USB_CONFIG_SIZE    (9 + 9 + 7 + 7)  /*!< config descriptor size */
#define WINUSB_MAX_MPS     512
#define WCID_VENDOR_CODE   0x17
#define ADB_INTF_NUM       0

#ifdef CONFIG_USBDEV_ADVANCE_DESC
static const uint8_t WCID_StringDescriptor_MSOS[18] = {
    ///////////////////////////////////////
    /// MS OS string descriptor
    ///////////////////////////////////////
    0x12,                                                   /* bLength */
    USB_DESCRIPTOR_TYPE_STRING,                             /* bDescriptorType */
    /* MSFT100 */
    'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0, /* wcChar_7 */
    WCID_VENDOR_CODE,                                       /* bVendorCode */
    0x00,                                                   /* bReserved */
};
static const uint8_t WINUSB_WCIDDescriptor[40] = {
    ///////////////////////////////////////
    /// WCID descriptor
    ///////////////////////////////////////
    0x28, 0x00, 0x00, 0x00,                   /* dwLength */
    0x09, 0x04,                               /* bcdVersion */
    0x04, 0x00,                               /* wIndex */
    0x01,                                     /* bCount */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* bReserved_7 */

    ///////////////////////////////////////
    /// WCID function descriptor
    ///////////////////////////////////////
    ADB_INTF_NUM, /* bFirstInterfaceNumber */
    0x01, /* bReserved */
    /* Compatible ID */
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, /* cCID_8: WINUSB */
    /*  */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* cSubCID_8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             /* bReserved_6 */
};
static const uint8_t WINUSB_IF0_WCIDProperties[142] = {
    ///////////////////////////////////////
    /// WCID property descriptor
    ///////////////////////////////////////
    0x8e, 0x00, 0x00, 0x00, /* dwLength */
    0x09, 0x04,             /* bcdVersion */
    0x05, 0x00,             /* wIndex */
    0x01, 0x00,             /* wCount */

    ///////////////////////////////////////
    /// registry propter descriptor
    ///////////////////////////////////////
    0x84, 0x00, 0x00, 0x00, /* dwSize */
    0x01, 0x00, 0x00, 0x00, /* dwPropertyDataType */
    0x28, 0x00,             /* wPropertyNameLength */
    /* DeviceInterfaceGUID */
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 'I', 0, 'n', 0, /* wcName_20 */
    't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0, 'G', 0, /* wcName_20 */
    'U', 0, 'I', 0, 'D', 0, 0x00, 0x00,                             /* wcName_20 */
    0x4e, 0x00, 0x00, 0x00, /* dwPropertyDataLength */
    /* {1D4B2365-4749-48EA-B38A-7C6FDDDD7E26} */
    '{', 0, '1', 0, 'D', 0, '4', 0, 'B', 0, '2', 0, '3', 0, '6', 0, /* wcData_39 */
    '5', 0, '-', 0, '4', 0, '7', 0, '4', 0, '9', 0, '-', 0, '4', 0, /* wcData_39 */
    '8', 0, 'E', 0, 'A', 0, '-', 0, 'B', 0, '3', 0, '8', 0, 'A', 0, /* wcData_39 */
    '-', 0, '7', 0, 'C', 0, '6', 0, 'F', 0, 'D', 0, 'D', 0, 'D', 0, /* wcData_39 */
    'D', 0, '7', 0, 'E', 0, '2', 0, '6', 0, '}', 0, 0x00, 0x00,     /* wcData_39 */
};
static const uint8_t *WINUSB_IFx_WCIDProperties[] = { WINUSB_IF0_WCIDProperties };
static struct usb_msosv1_descriptor msosv1_desc = {
    .string = WCID_StringDescriptor_MSOS,
    .vendor_code = WCID_VENDOR_CODE,
    .compat_id = WINUSB_WCIDDescriptor,
    .comp_id_property = WINUSB_IFx_WCIDProperties,
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    static const uint8_t device_descriptor[] = {
        USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01)
    };
    return device_descriptor;
}
static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    static const uint8_t config_descriptor[] = {
        USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, 100),
        ADB_DESCRIPTOR_INIT(ADB_INTF_NUM, WINUSB_IN_EP, WINUSB_OUT_EP, WINUSB_MAX_MPS)
    };
    return config_descriptor;
}
static const uint8_t *quality_descriptor_callback(uint8_t speed)
{
    static const uint8_t quality_descriptor[] = {
        0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
    };
    return quality_descriptor;
}
static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    static const char *string_descriptors[] = {
        "\x09\x04",             /* LangID */
        "ListenAi",             /* Manufacturer */
        "ADB recovery mode",    /* Product */
        "2025123456",           /* SerialNumber */
        "ADB Interface",        /* Interface */
    };
    
    if (index > 4) {
        return NULL;
    } else if (index == 3) {
        extern const char *device_id_str_get(void);
        static char sn[32] = {0};
        snprintf(sn, sizeof(sn), "BOOT-%s", device_id_str_get());
        return sn;
    }
    return string_descriptors[index];
}
#endif//CONFIG_USBDEV_ADVANCE_DESC

static void usb_adb_init(uint8_t busid, struct usbd_interface *intf, struct usb_descriptor *desc)
{
    desc->device_descriptor_callback = device_descriptor_callback;
    desc->config_descriptor_callback = config_descriptor_callback;
    desc->device_quality_descriptor_callback = quality_descriptor_callback;
    desc->string_descriptor_callback = string_descriptor_callback;
    desc->msosv1_descriptor = &msosv1_desc;
    usbd_adb_init_intf(busid, intf, WINUSB_IN_EP, WINUSB_OUT_EP);
}

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
    case USBD_EVENT_ERROR:
        LOGI("[USB] error");
        break; // USB error reported by the controller
    case USBD_EVENT_RESET:
        LOGI("[USB] reset");
        break; // USB reset
    case USBD_EVENT_SOF:
        LOGI("[USB] sof");
        break; // Start of Frame received
    case USBD_EVENT_CONNECTED:
        LOGI("[USB] connect");
        break; // USB connected
    case USBD_EVENT_DISCONNECTED:
        LOGI("[USB] disconn");
        break; // USB disconnected
    case USBD_EVENT_SUSPEND: {
        LOGI("[USB] suspend");
        LOGI("[USB] reboot system...");
        extern void reboot_hardware(void);
        reboot_hardware();
    } break; // USB connection suspended by the HOST
    case USBD_EVENT_RESUME:
        LOGI("[USB] resume");
        break; // USB connection resumed by the HOST
    case USBD_EVENT_CONFIGURED:
        LOGI("[USB] configue");
        break; // USB configuration done
    case USBD_EVENT_SET_INTERFACE:
        LOGI("[USB] interface");
        break; // USB interface selected
    case USBD_EVENT_SET_REMOTE_WAKEUP:
        LOGI("[USB] setwakeup");
        break; // USB set remote wakeup
    case USBD_EVENT_CLR_REMOTE_WAKEUP:
        LOGI("[USB] clrwakeup");
        break; // USB clear remote wakeup
    case USBD_EVENT_INIT:
        LOGI("[USB] init");
        break; // USB init done when call usbd_initialize
    case USBD_EVENT_DEINIT:
        LOGI("[USB] deinit");
        break; // USB deinit done when call usbd_deinitialize
    case USBD_EVENT_UNKNOWN:
        LOGI("[USB] unknown");
        break;
    default:
        LOGI("[USB] undef");
        break;
    }
}

void usb_task_init(void)
{
    LOGD("usb_adb_task start");
    const uint8_t busid = 0;
    struct usbd_interface *const intf = calloc(1, sizeof(struct usbd_interface));
    struct usb_descriptor *const desc = calloc(1, sizeof(struct usb_descriptor));
    ASSERT(intf && desc, "alloc failed");

#ifdef CONFIG_BOOT_ADB_SHELL
    adb_shell_init();
#endif
#ifdef CONFIG_BOOT_ADB_SYNC
    adb_sync_init();
#endif

    usbd_desc_register(busid, desc);
    usb_adb_init(busid, intf, desc);
    usbd_add_interface(busid, intf);
    usbd_initialize(busid, (uintptr_t)USBC_BASE, usbd_event_handler);

    // LOGD("usb_adb_task sleep");
    // vTaskDelay(portMAX_DELAY);
    // LOGD("usb_adb_task end");

    // usbd_deinitialize(busid);
    // free(intf);
    // free(desc);
    // vTaskDelete(NULL);
}

