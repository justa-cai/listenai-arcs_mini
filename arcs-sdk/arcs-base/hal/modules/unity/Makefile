TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libunity.a
TARGET	= 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=


#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	= 

CFLAGS  = -I $(TOPDIR)/include/driver \
          -I $(TOPDIR)/include/urpc \
          -I $(TOPDIR)/include/CMSIS \
          -I $(TOPDIR)/include/CMSIS/core \
          -I $(TOPDIR)/modules/unity/include \
		  -I $(TOPDIR)/chip/${CHIP}/include \
		  -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		  -I ./ \
          -DUNITY_INCLUDE_CONFIG_H

include $(TOPDIR)/rules.mk

