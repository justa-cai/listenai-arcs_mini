TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libluna_sim.a
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
CSRCS	= $(shell find . -name "*.c")
CPPSRCS	=
SSRCS	= 


CFLAGS  = -I $(TOPDIR)/include/rtos \
          -I $(TOPDIR)/include/driver \
          -I $(TOPDIR)/include/urpc \
          -I $(TOPDIR)/include/luna_sim \
          -I $(TOPDIR)/include \
          -I ./common \
          -I ./fft \
          -I ./ \
          -I ../

include $(TOPDIR)/rules.mk

