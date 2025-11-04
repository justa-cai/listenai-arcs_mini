TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libwlif.a
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

CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS  = -I$(TOPDIR)/include/NMSIS/Core/Include \
          -I$(TOPDIR)/include/bsp \
          -I$(TOPDIR)/include/rtos \
          -I$(TOPDIR)/chip/${CHIP}/include \
          -I$(TOPDIR)/chip/$(CHIP)/include/utils\
          -I$(TOPDIR)/chip/${CHIP}/ipc/ipc_core\
          -I$(TOPDIR)/chip/$(CHIP)/ipc/ipc_utils\
          -I$(TOPDIR)/chip/$(CHIP)/include/ipc\
          -I$(TOPDIR)/chip/$(CHIP)/include/net\
          -I$(TOPDIR)/chip/$(CHIP)/include/wifi\
          -I$(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
          -I$(TOPDIR)/chip/$(CHIP)/lwip/port/include \
          -I$(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include\

ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif

include $(TOPDIR)/rules.mk

