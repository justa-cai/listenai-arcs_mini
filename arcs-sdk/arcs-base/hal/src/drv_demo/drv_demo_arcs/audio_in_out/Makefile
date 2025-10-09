TOPDIR=../../../..

#ifeq ($(strip $(OS_DEF)),-DXOS_USED)
#$(info "Make nothing when XOS_USED is define")
#else

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = audio_in_out

# modules depend by this module
MODULES = bsp driver 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
LIBS	= -lbsp 

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
SSRCS	= 

#includings and flags
CFLAGS = -I $(TOPDIR)/include/NMSIS/Core/Include \
         -I $(TOPDIR)/include/NMSIS/DSP/Include \
         -I $(TOPDIR)/include/NMSIS/DSP/PrivateInclude \
         -I $(TOPDIR)/include/rtos \
		 -I $(TOPDIR)/chip/${CHIP}/bsp \
         -I $(TOPDIR)/chip/${CHIP}/include \
         -I $(TOPDIR)/chip/${CHIP}/include/register \
		 -I $(TOPDIR)/include/patch \
		 -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/include/debug \
		 -I $(TOPDIR)/include

#CFLAGS += -DPLATFORM_CP

#endif

include $(TOPDIR)/rules.mk

#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld

#LDSCRIPT  = my_flash2ram.ld
LDSCRIPT  = my_ram.ld

# add image header (image size & header checksum)
# the header is used when burned onto flash and can be idenfied by boot ROM code.
#
ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
