TOPDIR=../..

APP_NAME := vrtc

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libvrtc.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs= include test

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS = -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/chip/${CHIP}/include  \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I$(TOPDIR)/chip/${CHIP}/bsp

include $(TOPDIR)/rules.mk
