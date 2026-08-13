TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= dvp

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
CSRCS	+= $(shell find ./dma2d -name "*.c")
CSRCS	+= $(shell find ./dvp -name "*.c")
CSRCS	+= $(shell find ./i2c -name "*.c")
CSRCS	+= $(shell find ./camera -name "*.c")

CPPSRCS	=
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS = -I . \
		 -I ./dma2d \
		 -I ./dvp \
		 -I ./i2c \
		 -I ./camera \
		 -I ./camera/sensors \
		 -I ./camera/sensors/inc
		 
export CONFIG_FREERTOSCONFIG_H_PATH=chip/venusa/demo/dvp
export CFG_RTOS=1
export CONFIG_USE_SMP=1
export IC_BOARD=1

include $(TOPDIR)/rules.mk


LDSCRIPT = ./ram.ld
# LDSCRIPT = ./flash.ld


# ext:
# 	@sh $(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin	
