TOPDIR=../../..

# which usb subsytem is selected? default NONE.
# NONE: No USB_SUBSYS, TINY: TinyUSB, CHERRY: CherryUSB
CONFIG_USB_SUBSYS ?= NONE
# which usb controller driver is selected? default LISA
CONFIG_USBC_TYPE ?= LISA

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
ifeq ($(IC_BOARD), 0)
LIB		= libdrv_fpga.a
else
LIB		= libdrv.a
endif

TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= 
ifeq ($(IC_BOARD), 0)
LIBS	+= 
else
LIBS	+= 
endif

#exclude subdirs
exclude_dirs = $(patsubst ./%,%,$(shell find . -maxdepth 5 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
#CSRCS	= $(shell find . -name "*.c")
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS =  

ifeq ($(CONFIG_USB_SUBSYS), TINY) # TinyUSB
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys
CSRCS = $(shell find . -path ./usb_subsys_cherry -prune -o -iname "*.c" -print)

#decide to include the application specific tusb_config.h or default one
ifneq ($(strip $(CFG_TUSB_CONFIG_H_PATHNAME)),)
$(info "PRIVATE tusb_config.h @ $(CFG_TUSB_CONFIG_H_PATHNAME)")
#absolute path
CFLAGS += -DCFG_TUSB_CONFIG_FILE=\"$(CFG_TUSB_CONFIG_H_PATHNAME)\"
else
$(info "PUBLIC tusb_config.h @ $(TOPDIR)/chip/${CHIP}/driver/usb_subsys/tusb_config.h")
endif

else
ifeq ($(CONFIG_USB_SUBSYS), CHERRY) # CherryUSB
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry \
          -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/core \
          -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/common \
          -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/class/hub \
          -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/class/cdc
CSRCS = $(shell find . -path ./usb_subsys -prune -o \
                       -path ./usb_subsys_cherry/port -prune -o \
                       -iname "*.c" -print)
ifeq ($(CONFIG_USBC_TYPE), LISA) # use "lisa" controller driver
CSRCS += $(shell find ./usb_subsys_cherry/port/lisa -name "*.c")
else # [default] use "musb" controller driver
CSRCS += $(shell find ./usb_subsys_cherry/port/musb -name "*.c")
endif

else # NO USB_SUBSYS
CSRCS = $(shell find . \( -path ./usb_subsys -o -path ./usb_subsys_cherry \) -prune -o -iname "*.c" -print)
endif
endif # USB_SUBSYS

ifeq ($(CONFIG_SEGGER_SYSVIEW), 1)
CFLAGS += -I $(TOPDIR)/modules/segger_rtt/include
CFLAGS += -I $(TOPDIR)/$(CONFIG_FREERTOSCONFIG_H_PATH)/segger_sysview_freertos
endif

include $(TOPDIR)/rules.mk

