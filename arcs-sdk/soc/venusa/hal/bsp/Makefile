TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libbsp.a
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

CSRCS	= $(wildcard *.c) $(wildcard gcc/*.c) $(wildcard gcc/stubs/*.c)
CPPSRCS	=
SSRCS	= $(wildcard gcc/*.S)

#includings and flags
CFLAGS  = 

include $(TOPDIR)/rules.mk

