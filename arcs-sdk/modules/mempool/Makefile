TOPDIR	= ../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libmempool.a
TARGET	= 
LDS		= 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
EXDIRS	=

#files want to build: eg. CSRCS=x.c  SSRCS=x.S
CSRCS	= $(shell find . -maxdepth 5 -type f -name "*.c")
SSRCS	= 
# OPTIM	= -Os

#includings and flags
CFLAGS += -I./include/
CFLAGS += -I$(TOPDIR)/include/lisa_porting/lisa_common
CFLAGS += -I$(TOPDIR)/include/lisa_porting/lisa_os
CFLAGS += -I$(TOPDIR)/include/lisa_porting/lisa_log

CFLAGS += -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/modules/driver/private_include/usb \
		 $(shell find $(TOPDIR)/include $(TOPDIR)/src -type d -exec echo "-I{}/ " \;)

include $(TOPDIR)/rules.mk
