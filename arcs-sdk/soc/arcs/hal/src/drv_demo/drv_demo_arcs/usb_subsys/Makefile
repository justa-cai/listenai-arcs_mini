TOPDIR=../../../..

USB_BTH = 0
USB_HID = 0
USB_AUDIO = 0
USB_CDC = 1
USB_CDC_HID = 1
USB_MSC = 0
USB_VENDOR = 1
USB_VENDOR_DBG = 1

#SUBDIRS = usb_cdc_vcom
#SUBDIRS = usb_vendor_echo_dbg

#ifeq ($(USB_BTH), 0)
#exclude_dirs += usb_bth
#endif

#ifeq ($(USB_HID), 0)
#exclude_dirs += usb_hid_csm usb_audio_combo
#endif

#ifeq ($(USB_AUDIO), 0)
#exclude_dirs += usb_audio_mic usb_audio_spk usb_audio_io usb_audio_combo
#endif

ifeq ($(USB_CDC), 0)
exclude_dirs += usb_cdc_vcom #usb_audio_combo
endif

ifeq ($(USB_CDC_HID), 0)
exclude_dirs += usb_cdc_hid #usb_audio_combo
endif

#ifeq ($(USB_MSC), 0)
#exclude_dirs += usb_msc_ram usb_audio_io
#endif

ifeq ($(USB_VENDOR), 0)
exclude_dirs += usb_vendor_echo #usb_audio_io
endif

ifeq ($(USB_VENDOR_DBG), 0)
exclude_dirs += usb_vendor_echo_dbg #usb_audio_io
endif

include $(TOPDIR)/rules.mk

