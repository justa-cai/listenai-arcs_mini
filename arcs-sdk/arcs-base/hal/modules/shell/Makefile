TOPDIR=../..

APP_NAME := FHOST
DATE := $(shell date '+%Y%m%d-%H%M%S')
HOSTNAME := $(shell hostname)
VERSION := "Build on: $(HOSTNAME)\r\nRelease version: $(DATE)"

VERSION_FILE := sw_version.h

ifeq (,$(wildcard $(VERSION_FILE)))
$(shell touch $(VERSION_FILE))
endif

$(shell echo "#define $(APP_NAME)_VERSION \"$(VERSION)\"" > $(VERSION_FILE))

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libshell.a
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
CFLAGS = -I$(TOPDIR)/include/debug \
		 -I$(TOPDIR)/include/driver \
		 -I$(TOPDIR)/chip/${CHIP}/include  \
		 -I$(TOPDIR)/include/bsp \
		 -I$(TOPDIR)/include/nvs \
		 -I$(TOPDIR)/include/CMSIS \
		 -I$(TOPDIR)/include/CMSIS/core \
		 -I$(TOPDIR)/modules/driver/private_include \
		 -I$(TOPDIR)/chip/${CHIP}/driver/private_include


#CMTID = $(shell git rev-parse --short HEAD 2>/dev/null)
#CFLAGS += -DLIB_NM=\"$(LIB)\" -DCMT_ID=\"$(SDK_VER)\($(CMTID)\)\"
#ext:
#	@touch version.c

include $(TOPDIR)/rules.mk
