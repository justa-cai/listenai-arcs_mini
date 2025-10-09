TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libprf.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c) $(wildcard hogp/hogpd/src/*.c) $(wildcard bas/bass/src/*.c)  $(wildcard dis/diss/src/*.c)
CSRCS   += $(wildcard ota/otas/src/*.c)
ifeq ($(PROFILE_NETCFG), 1)
CSRCS   += $(wildcard netcfg_ble/netcfg_bles/src/*.c)
endif
ifeq ($(PROFILE_BASC), 1)
CSRCS   += $(wildcard bas/basc/src/*.c)
endif
ifeq ($(PROFILE_HOGPRH), 1)
CSRCS   += $(wildcard hogp/hogprh/src/*.c)
endif
ifeq ($(PROFILE_DISC), 1)
CSRCS   += $(wildcard dis/disc/src/*.c)
endif
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS = -I$(TOPDIR)/include/rtos \
		 -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/chip/${CHIP}/include  \
		 -I$(TOPDIR)/chip/${CHIP}/btd/bt_inc  \
		 -I$(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc  \
		 -I$(TOPDIR)/chip/${CHIP}/include/wifi \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/include/nvs \
		 -I$(TOPDIR)/include/CMSIS \
		 -I$(TOPDIR)/include/CMSIS/core \
		 -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/modules/profiles/hogp \
		 -I$(TOPDIR)/modules/profiles/hogp/hogpd/api \
		 -I$(TOPDIR)/modules/profiles/hogp/hogprh/api \
		 -I$(TOPDIR)/modules/profiles/bas/bass/api \
		 -I$(TOPDIR)/modules/profiles/bas/basc/api \
		 -I$(TOPDIR)/modules/profiles/dis/diss/api \
		 -I$(TOPDIR)/modules/profiles/dis/disc/api \
		 -I$(TOPDIR)/modules/profiles/ota/otas/api \
		 -I$(TOPDIR)/modules/profiles/netcfg_ble/netcfg_bles/api \
		 -I$(TOPDIR)/src/${CHIP}/${TGT}/ \
		 -I$(TOPDIR)/src/bt/${TGT}/ \
		 -I$(TOPDIR)/src/bt/audio_simu/ \
		 -I$(TOPDIR)/src/bt/simulator/ \
		 -I$(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I./

#CMTID = $(shell git rev-parse --short HEAD 2>/dev/null)
#CFLAGS += -DLIB_NM=\"$(LIB)\" -DCMT_ID=\"$(SDK_VER)\($(CMTID)\)\"
#ext:
#	@touch version.c

include $(TOPDIR)/rules.mk
