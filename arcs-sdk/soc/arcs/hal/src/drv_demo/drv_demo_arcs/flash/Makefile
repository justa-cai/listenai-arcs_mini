TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= flash_test

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
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS = -I$(TOPDIR)/include/rtos \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/CMSIS \
		 -I$(TOPDIR)/include/CMSIS/core \
		 -I $(TOPDIR)/chip/${CHIP}/include

include $(TOPDIR)/rules.mk
#overwrite
LDSCRIPT = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
#LDSCRIPT  = ./flash_ramcode.ld