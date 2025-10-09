TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= qdec

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
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS = 

include $(TOPDIR)/rules.mk
LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld

