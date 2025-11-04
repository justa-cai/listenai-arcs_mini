TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libbtos.a
TARGET  =

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS    =
ifeq ($(IC_BOARD), 0)
LIBS    +=
else
LIBS    +=
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S

CSRCS	= $(shell find . -name "*.c")
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include \
		  -I $(TOPDIR)/include/debug \
		  -I $(TOPDIR)/include/bsp \
		  -I $(TOPDIR)/include/driver \
		  -I $(TOPDIR)/include/rtos \
		  -I $(TOPDIR)/chip/${CHIP}/bsp \
		  -I $(TOPDIR)/chip/${CHIP}/include \
		  -I $(TOPDIR)/chip/${CHIP}/include/wifi \
		  -I $(TOPDIR)/chip/${CHIP}/include/wifi/common \
		  -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		  -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
		  -I $(TOPDIR)/chip/${CHIP}/btos \
		  -I $(TOPDIR)/chip/${CHIP}/btos/task/api \
		  -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
		  -I $(TOPDIR)/include/nvs


include $(TOPDIR)/rules.mk

