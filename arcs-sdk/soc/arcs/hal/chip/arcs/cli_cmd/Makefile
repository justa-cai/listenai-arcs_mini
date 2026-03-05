TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libclicmd.a
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

LWIP_VER := $(LWIP_VER)
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
          -I $(TOPDIR)/chip/${CHIP}/include/bt \
          -I $(TOPDIR)/chip/${CHIP}/include/wifi \
          -I $(TOPDIR)/chip/${CHIP}/include/net \
          -I $(TOPDIR)/chip/${CHIP}/include/utils \
          -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_core \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/ipc_utils \
          -I $(TOPDIR)/chip/${CHIP}/include/ipc \
          -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
          -I $(TOPDIR)/chip/${CHIP}/wota \
          -I $(TOPDIR)/chip/${CHIP}/atcmd \
          -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
          -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include \
          -I $(TOPDIR)/chip/$(CHIP)/lwip/port/include \
          -I $(TOPDIR)/src/${APP_PATH}


CFLAGS += -DWIFI_OTA=$(WIFI_OTA)

#macro ctrl
ifeq ($(strip $(WCN_TYPE)), both)
CFLAGS += -DCLI_TYPE_WF
else ifeq ($(strip $(WCN_TYPE)), wf)
CFLAGS += -DCLI_TYPE_WF
else ifeq ($(strip $(WCN_TYPE)), bt)

endif

ifeq ($(WIFI_RAM_ATE), 1)
CFLAGS += -DWIFI_RAM_ATE
endif

ifeq ($(CFG_RF_SELFCALI_FROM_NV), 1)
CFLAGS += -DRF_SELF_CALI_FROM_NV
endif
ifeq ($(CFG_RF_SELFCALI_WRITE_TO_NV), 1)
CFLAGS += -DRF_SELF_CALI_WRITE_TO_NV
endif
ifeq ($(CFG_USE_FLASH_OTP), 1)
CFLAGS += -DUSE_FLASH_OTP=1
endif
CFG_FLASH_NOR_OTP_NV_BASE_ADDR ?= 0x30200000
CFLAGS += -DFLASH_NOR_OTP_NV_BASE_ADDR=$(CFG_FLASH_NOR_OTP_NV_BASE_ADDR)

include $(TOPDIR)/rules.mk

