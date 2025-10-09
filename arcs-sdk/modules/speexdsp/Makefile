TOPDIR	= ../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libspeexdsp.a
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
CFLAGS += -Dmalloc=lisa_mem_alloc
CFLAGS += -Dcalloc=lisa_mem_calloc
CFLAGS += -Drealloc=lisa_mem_realloc
CFLAGS += -Dfree=lisa_mem_free
CFLAGS += -Dsram_malloc=lisa_mem_sram_alloc
CFLAGS += -Dsram_free=lisa_mem_sram_free
CFLAGS += -DFIXED_POINT -DEXPORT=
CFLAGS += -O3 -w -funwind-tables -fcommon

CFLAGS += -I./
CFLAGS += -I./speex

CFLAGS += -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/modules/driver/private_include/usb \
		 $(shell find $(TOPDIR)/include $(TOPDIR)/src -type d -exec echo "-I{}/ " \;)

#CFLAGS += -I$(TOPDIR)/include/audioplayer/lisa_porting/include/lisa_os

include $(TOPDIR)/rules.mk
