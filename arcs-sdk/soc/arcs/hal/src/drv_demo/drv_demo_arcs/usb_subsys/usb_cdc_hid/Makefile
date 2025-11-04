#check if CDC demo is included or not
ifeq ($(USB_CDC_HID), 0)
$(info "Make nothing when USB_CDC_HID set to 0")
else

TOPDIR=../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = usb_cdc_hid

MODULES = bsp  rtos driver

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
LIBS	= -lbsp -lrtos

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=$(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	= 
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS = -I $(TOPDIR)/include/NMSIS \
         -I $(TOPDIR)/include/NMSIS/core \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys/class/cdc \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys/class/vendor \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys/class/hid \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/include

#export the absolute path of tusb_config.h
#CFG_TUSB_CONFIG_H_PATHNAME for usb_subsys compiling
export CFG_TUSB_CONFIG_H_PATHNAME = $(abspath ./tusb_config.h)
CFLAGS  += -DCFG_TUSB_CONFIG_FILE=\"$(CFG_TUSB_CONFIG_H_PATHNAME)\"
		 
CFG_RTOS=1
export CFG_RTOS
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

include $(TOPDIR)/rules.mk

#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
LDSCRIPT  = ./ram.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld
#ext:
#      $(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin

endif # $(USB_CDC_HID)
