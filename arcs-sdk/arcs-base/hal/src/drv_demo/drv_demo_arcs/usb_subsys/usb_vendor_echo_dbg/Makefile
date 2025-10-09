#check if Vendor-defined demo is included or not
ifeq ($(USB_VENDOR), 0)
$(info "Make nothing when USB_VENDOR set to 0")
else

TOPDIR=../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = usb_vendor_echo_dbg

# modules depend by this module
MODULES = bsp driver  rtos #usb_subsys

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
LIBS	= -lbsp -lrtos #-lusb_subsys

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
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys

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

LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld
#LDSCRIPT  = my_flash2ram.ld
#LDSCRIPT  = my_ram.ld
#LDSCRIPT  = ./ram.ld

# add image header (image size & header checksum)
# the header is used when burned onto flash and can be idenfied by boot ROM code.
#
ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin

endif # $(USB_VENDOR)
