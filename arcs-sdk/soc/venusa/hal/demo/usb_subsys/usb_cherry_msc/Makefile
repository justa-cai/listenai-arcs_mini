TOPDIR=../../../../..

# which usb subsytem is selected? default NONE.
# NONE: No USB_SUBSYS, TINY: TinyUSB, CHERRY: CherryUSB
export CONFIG_USB_SUBSYS = CHERRY

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = usb_cherry

MODULES = bsp rtos driver

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
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/class/msc \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/common \
		 -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys_cherry/core \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/include


		 
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
