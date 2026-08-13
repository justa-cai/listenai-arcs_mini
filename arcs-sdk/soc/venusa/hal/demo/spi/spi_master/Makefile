TOPDIR=../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = spi_master

# modules depend by this module
MODULES = bsp driver unity

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -ldbg -lbsp -lopi -lmva
LIBS	= -lbsp -lunity

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I . \
         -I $(TOPDIR)/chip/$(CHIP)/modules/unity/include


include $(TOPDIR)/rules.mk
