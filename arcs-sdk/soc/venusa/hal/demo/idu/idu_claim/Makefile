TOPDIR=../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = idu_claim

# modules depend by this module
MODULES = bsp driver unity

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp -lunity

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
#exclude_dirs=$(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I . \
		 -I $(TOPDIR)/chip/$(CHIP)/modules/unity/include

export CONFIG_USE_SMP=1

include $(TOPDIR)/rules.mk
LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
# LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/flash.ld
