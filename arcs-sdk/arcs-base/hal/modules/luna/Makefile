TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libluna.a
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

#-mcoproc
CFLAGS  = -I $(TOPDIR)/include/debug \
          -I $(TOPDIR)/include/luna \
          -I $(TOPDIR)/include \
          -I ./cmd \
          -I ./isa \
          -I ./ \
          -I ../

include $(TOPDIR)/rules.mk

