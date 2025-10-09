TOPDIR=../../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= adc_8k_100x

# modules depend by this module
MODULES = bsp driver 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp 
ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c) $(wildcard ../../*.c)
CPPSRCS	=
SSRCS	= $(wildcard *.S)

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

BUILD_ROM=1
include $(TOPDIR)/rules.mk

