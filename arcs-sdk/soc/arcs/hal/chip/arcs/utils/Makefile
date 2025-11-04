TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libutils.a
TARGET  =

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS    =
ifeq ($(IC_BOARD), 0)
LIBS    +=
else
LIBS    +=
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S

CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/rtos \
          -I $(TOPDIR)/chip/${CHIP}/include \
          -I $(TOPDIR)/chip/${CHIP}/include/utils \
          -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
	  -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure


include $(TOPDIR)/rules.mk

