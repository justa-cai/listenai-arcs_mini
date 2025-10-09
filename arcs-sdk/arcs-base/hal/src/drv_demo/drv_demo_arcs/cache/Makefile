TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = cache
#TARGET = dma_xos_chk

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
CFLAGS = -I $(TOPDIR)/include/CMSIS \
         -I $(TOPDIR)/include/CMSIS/core \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/include/debug \
		 -I $(TOPDIR)/include \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/include

include $(TOPDIR)/rules.mk
LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld