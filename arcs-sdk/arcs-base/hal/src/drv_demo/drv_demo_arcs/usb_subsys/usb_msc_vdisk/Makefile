#check if MSC demo is included or not
ifeq ($(USB_MSC), 0)
$(info "Make nothing when USB_MSC set to 0")
else

TOPDIR=../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = usb_msc_vdisk

# modules depend by this module
MODULES = bsp driver  rtos

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -ldbg -lbsp -lopi -lmva
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
CFLAGS += -I $(TOPDIR)/include/NMSIS \
         -I $(TOPDIR)/include/NMSIS/core \
         -I $(TOPDIR)/chip/${CHIP}/bsp \
         -I $(TOPDIR)/chip/${CHIP}/include \
         -I $(TOPDIR)/chip/${CHIP}/include/register \
		 -I $(TOPDIR)/include/rtos \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys

#export the absolute path of tusb_config.h
#CFG_TUSB_CONFIG_H_PATHNAME for usb_subsys compiling
export CFG_TUSB_CONFIG_H_PATHNAME = $(abspath ./tusb_config.h)
CFLAGS  += -DCFG_TUSB_CONFIG_FILE=\"$(CFG_TUSB_CONFIG_H_PATHNAME)\"

export CFG_RTOS = 1
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

include $(TOPDIR)/rules.mk

LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld

# add image header (image size & header checksum)
# the header is used when burned onto flash and can be idenfied by boot ROM code.
#
ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin

endif # $(USB_MSC)
