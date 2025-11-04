TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= liburpc_server.a
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
CSRCS	= urpc.c urpc_server.c urpc_mbox.c urpc_mbox_server.c
CPPSRCS	=
SSRCS	= 

CFLAGS  = -I . \
          -I $(TOPDIR)/chip/${CHIP}/include/urpc \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/debug \
          -I $(TOPDIR)/include/driver

include $(TOPDIR)/rules.mk

