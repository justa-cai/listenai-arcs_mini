TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libls_jpeg.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
ifeq ($(CFG_RTOS), 1)
MODULES = rtos
LIBS	= -lrtos
endif

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

include $(TOPDIR)/rules.mk
