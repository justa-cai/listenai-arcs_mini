TOPDIR=../../..

# which usb subsytem is selected? default NONE.
# NONE: No USB_SUBSYS, TINY: TinyUSB, CHERRY: CherryUSB
CONFIG_USB_SUBSYS ?= NONE
# which usb controller driver is selected? default LISA
CONFIG_USBC_TYPE ?= MUSB

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
exclude_dirs= $(patsubst ./%,%,$(shell find . -maxdepth 3 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
#CSRCS	= $(shell find . -name "*.c")
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS =  -I $(TOPDIR)/include/NMSIS/Core/Include \
          -I $(TOPDIR)/chip/${CHIP}/bsp \
          -I $(TOPDIR)/chip/${CHIP}/include \
          -I $(TOPDIR)/chip/${CHIP}/include/register \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/bt/plf/api \
          -I $(TOPDIR)/modules/ota/include
          
CFLAGS += -DIC_USE_RTOS=1 -DIC_HAL_DCACHE_SIZE=32 -DIC_HAL_DCACHE_IS_COHERENT=1

ifeq ($(CONFIG_USB_SUBSYS), TINY) # TinyUSB
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys
CSRCS = $(shell find . -path ./usb_subsys_cherry -prune -o -iname "*.c" -print)

#decide to include the application specific tusb_config.h or default one
ifneq ($(strip $(CFG_TUSB_CONFIG_H_PATHNAME)),)
$(info "PRIVATE tusb_config.h @ $(CFG_TUSB_CONFIG_H_PATHNAME)")
#absolute path
CFLAGS  += -DCFG_TUSB_CONFIG_FILE=\"$(CFG_TUSB_CONFIG_H_PATHNAME)\"
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

#decide to include the application specific usb_config.h
ifneq ($(strip $(CFG_USB_CONFIG_H_PATH)),)
$(info "usb_config.h @ $(CFG_USB_CONFIG_H_PATH)")
#absolute path
CFLAGS  += -I $(CFG_USB_CONFIG_H_PATH)
endif

ifeq ($(CONFIG_USBC_TYPE), LISA) # use "lisa" controller driver
CSRCS += $(shell find ./usb_subsys_cherry/port/lisa -name "*.c")
else # [default] use "musb" controller driver
CSRCS += $(shell find ./usb_subsys_cherry/port/musb -name "*.c")
endif

else # NO USB_SUBSYS
CSRCS = $(shell find . \( -path ./usb_subsys -o -path ./usb_subsys_cherry \) -prune -o -iname "*.c" -print)
endif
endif # USB_SUBSYS


ifneq (${CFG_RTOS},1)
# exclude files from ./amp_ipc/
CSRCS_TEMP := $(filter-out ./amp_ipc/%,$(CSRCS))
CSRCS	= $(CSRCS_TEMP)
endif # ifneq (${CFG_RTOS},1)


ifeq ($(FLASH_ENC_ALL), )
FLASH_ENC_ALL = 0
endif

ifeq ($(BOOT_HARTID), 0)
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/include/lsf_client \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_client/fifo \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_client/ic_mutex
else
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/include/lsf_server \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_server/fifo \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_server/ic_mutex
endif

CFLAGS += -DFLASH_ENC_ALL=${FLASH_ENC_ALL}

include $(TOPDIR)/rules.mk

