TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = helloworld

# modules depend by this module
MODULES = bsp driver

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -ldbg -lbsp -lopi -lmva
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
CFLAGS = -I $(TOPDIR)/chip/${CHIP}/bsp \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/include \
		 -I $(TOPDIR)/include/NMSIS/core/include \
		 -I $(TOPDIR)/include/debug

include $(TOPDIR)/rules.mk
LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/rom_core0.ld
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/rom_core1.ld