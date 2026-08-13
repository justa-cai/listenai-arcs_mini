TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB    = 
TARGET = dma_nos

# modules depend by this module
MODULES = bsp driver #debug

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp #-ldbg

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs = # $(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I $(TOPDIR)/chip/${CHIP}/include \
 		 -I $(TOPDIR)/chip/${CHIP}/include/register \
		 -I $(TOPDIR)/chip/${CHIP}/bsp \
		 -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/include/bsp

include $(TOPDIR)/rules.mk

LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
