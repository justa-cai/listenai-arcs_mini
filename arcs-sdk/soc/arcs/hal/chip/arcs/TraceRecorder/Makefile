TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libtrace.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=
ifeq ($(IC_BOARD), 0)
LIBS	+=
else
LIBS	+=
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c) $(wildcard streamports/Jlink_RTT/*.c)
CPPSRCS	=
SSRCS	=

CFLAGS  = -I ./include \
          -I ./config \
          -I ./streamports/Jlink_RTT/include \
          -I ./streamports/Jlink_RTT/config

include $(TOPDIR)/rules.mk
