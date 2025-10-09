TOPDIR=../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = spi_oled

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
CFLAGS = -I $(TOPDIR)/include/NMSIS \
         -I $(TOPDIR)/include/NMSIS/core \
		 -I $(TOPDIR)/chip/${CHIP}/bsp \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/include \
		 -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/include/debug \
		 -I $(TOPDIR)/include

#OPTMIZED_SIZE
#BUILD_NANO=1

include $(TOPDIR)/rules.mk
LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld