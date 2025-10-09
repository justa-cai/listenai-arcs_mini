TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libnvs.a
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
CFLAGS = -I$(TOPDIR)/include/rtos \
		 -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/chip/${CHIP}/include  \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/include/CMSIS \
		 -I$(TOPDIR)/include/CMSIS/core \
		 -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I$(TOPDIR)/src/$(APP_PATH)  \
		 -I$(TOPDIR)/modules/nvs/include	\
		 -I./

include $(TOPDIR)/rules.mk
