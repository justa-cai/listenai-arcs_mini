TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libaudio.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs= 

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c) $(wildcard aud_mgr/*.c) $(wildcard aud_pro/*.c) $(wildcard aud_alg/resample/*.c)
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
		 -I$(TOPDIR)/chip/${CHIP}/btd/bt_inc  \
		 -I$(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc  \
		 -I$(TOPDIR)/include/aud \
		 -I$(TOPDIR)/modules/audio/api \
		 -I$(TOPDIR)/modules/audio/aud_alg \
		 -I$(TOPDIR)/src/bt/bt_audio/api \
		 -I./

#CMTID = $(shell git rev-parse --short HEAD 2>/dev/null)
#CFLAGS += -DLIB_NM=\"$(LIB)\" -DCMT_ID=\"$(SDK_VER)\($(CMTID)\)\"
#ext:
#	@touch version.c

include $(TOPDIR)/rules.mk