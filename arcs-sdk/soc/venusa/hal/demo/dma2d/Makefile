TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = dma2d

# modules depend by this module
MODULES = bsp driver  unity

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp -lunity

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=$(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS  = $(shell find ./resource -name "*.c")
CSRCS += ./dvp_in.c
CSRCS += ./qspi_out.c
CSRCS += ./dma2d_check.c

#includings and flags
CFLAGS = -I $(TOPDIR)/include/NMSIS/Core/Include \
		 -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/include/debug \
		 -I $(TOPDIR)/include \
		 -I $(TOPDIR)/chip/${CHIP}/bsp \
		 -I $(TOPDIR)/chip/${CHIP}/include \

include $(TOPDIR)/rules.mk
#LDSCRIPT = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
LDSCRIPT  = ./ram.ld
