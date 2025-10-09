TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libsegger_rtt.a
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
CFLAGS = -I $(TOPDIR)/modules/segger_rtt/include \
         -I./

#CMTID = $(shell git rev-parse --short HEAD 2>/dev/null)
#CFLAGS += -DLIB_NM=\"$(LIB)\" -DCMT_ID=\"$(SDK_VER)\($(CMTID)\)\"
#ext:
#	@touch version.c

ifneq ($(strip $(CONFIG_RTT_BUFFER_SIZE_UP)),)
$(info "customized RTT buffer size: configured as $(CONFIG_RTT_BUFFER_SIZE_UP)")
CFLAGS += -DBUFFER_SIZE_UP=$(CONFIG_RTT_BUFFER_SIZE_UP)
endif

include $(TOPDIR)/rules.mk
