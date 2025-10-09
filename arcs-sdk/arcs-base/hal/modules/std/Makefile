TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libstd.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs= 

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I$(TOPDIR)/include/rtos \
		 -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/chip/{CHIP}/include  \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/include/nvs \
		 -I$(TOPDIR)/include/CMSIS \
		 -I$(TOPDIR)/include/CMSIS/core \
		 -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I./

#CMTID = $(shell git rev-parse --short HEAD 2>/dev/null)
#CFLAGS += -DLIB_NM=\"$(LIB)\" -DCMT_ID=\"$(SDK_VER)\($(CMTID)\)\"
#ext:
#	@touch version.c

include $(TOPDIR)/rules.mk