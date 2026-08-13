TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libls_dma2d.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
ifeq ($(IC_BOARD), 0)
LIBS	+=
else
LIBS	+=
endif

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS = -I. \
		 -I $(TOPDIR)/chip/${CHIP}/modules/fragment 

include $(TOPDIR)/rules.mk
