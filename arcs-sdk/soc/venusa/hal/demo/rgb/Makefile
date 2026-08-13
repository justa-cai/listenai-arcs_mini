TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= rgb

# modules depend by this module
MODULES = bsp driver rtos

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp -lrtos
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
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS = -I . 

export CONFIG_FREERTOSCONFIG_H_PATH=chip/venusa/demo/rgb
export CFG_RTOS=1
export CONFIG_USE_SMP=1
export IC_BOARD=1

include $(TOPDIR)/rules.mk


LDSCRIPT = ./ram.ld
# LDSCRIPT = ./flash.ld


# ext:
# 	@sh $(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin	
