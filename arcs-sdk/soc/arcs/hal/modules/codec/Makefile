TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libcodec.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs= 

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c) $(wildcard lc3/*.c) $(wildcard lc3/basic_op/*.c) $(wildcard sbc/*.c) $(wildcard msbc/*.c)
CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I$(TOPDIR)/include/rtos \
		 -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/chip/${CHIP}/include  \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/include/nvs \
		 -I$(TOPDIR)/include/CMSIS \
		 -I$(TOPDIR)/include/CMSIS/core \
		 -I$(TOPDIR)/modules/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/btd/bt_inc \
		 -I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc \
		 -I$(TOPDIR)/modules/codec/lc3 \
		 -I$(TOPDIR)/modules/codec/lc3/basic_op \
		 -I$(TOPDIR)/modules/codec/sbc \
		 -I$(TOPDIR)/modules/codec/msbc \
		 -I$(TOPDIR)/modules/audio/api \
		 -I$(TOPDIR)/modules/audio/aud_alg \
		 -I./

#CMTID = $(shell git rev-parse --short HEAD 2>/dev/null)
#CFLAGS += -DLIB_NM=\"$(LIB)\" -DCMT_ID=\"$(SDK_VER)\($(CMTID)\)\"
#ext:
#	@touch version.c

include $(TOPDIR)/rules.mk