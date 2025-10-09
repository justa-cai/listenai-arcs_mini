TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libatcmd.a
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
ifeq ($(strip $(WCN_TYPE)), both)
CSRCS	= atcmd.c atcmd_wifi.c atcmd_hash.c atcmd_iperf.c atcmd_lwip.c ping.c atcmd_bt.c atcmd_bt_if.c
else ifeq ($(strip $(WCN_TYPE)), wf)
CSRCS	= atcmd.c atcmd_wifi.c atcmd_hash.c atcmd_iperf.c atcmd_lwip.c ping.c
else ifeq ($(strip $(WCN_TYPE)), bt)
CSRCS	= atcmd.c atcmd_bt.c atcmd_bt_if.c
endif

#CSRCS	= $(shell find . -name "*.c")
CPPSRCS	=
SSRCS	=


LWIP_VER := lwip-2.2.0
#includings and flags
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include \
          -I $(TOPDIR)/include/debug \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/driver \
          -I $(TOPDIR)/include/rtos \
          -I $(TOPDIR)/chip/${CHIP}/bsp \
          -I $(TOPDIR)/chip/${CHIP}/btos \
          -I $(TOPDIR)/chip/${CHIP}/btos/task/api \
          -I $(TOPDIR)/chip/${CHIP}/include \
          -I $(TOPDIR)/chip/${CHIP}/include/net \
          -I $(TOPDIR)/chip/${CHIP}/include/wifi \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc \
          -I $(TOPDIR)/chip/${CHIP}/include/bt \
          -I $(TOPDIR)/chip/${CHIP}/include/wifi/common \
          -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
          -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
          -I $(TOPDIR)/chip/$(CHIP)/lwip/port/include \
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip \
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include \
	  -I $(TOPDIR)/chip/$(CHIP)/lwip/port/include \
          -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
          -I $(TOPDIR)/include/nvs \
          -I $(TOPDIR)/src/${APP_PATH} \
          -I $(TOPDIR)/chip/${CHIP}/cli_cmd


include $(TOPDIR)/rules.mk

