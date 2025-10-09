TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libwcnd.a

TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lnvs
ifeq ($(IC_BOARD), 0)
LIBS	+=
else
LIBS	+=
endif

#exclude subdirs
exclude_dirs= $(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))
#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
ifeq ($(strip $(WCN_TYPE)), both)
CSRCS	= $(wildcard *.c) $(wildcard sleep/*.c)  $(wildcard wf_drv/*.c) $(wildcard ext_rf/*.c) $(wildcard bt_drv/*.c) $(wildcard timer/*.c) $(wildcard dma/*.c) $(wildcard nv/*.c) $(wildcard misc/*.c)
else ifeq ($(strip $(WCN_TYPE)), wf)
CSRCS	= $(wildcard *.c) $(wildcard sleep/*.c)  $(wildcard wf_drv/*.c) $(wildcard nv/*.c) $(wildcard misc/*.c)
else ifeq ($(strip $(WCN_TYPE)), bt)
CSRCS	= $(wildcard *.c) $(wildcard sleep/*.c)  $(wildcard ext_rf/*.c) $(wildcard bt_drv/*.c) $(wildcard timer/*.c) $(wildcard dma/*.c) $(wildcard nv/*.c) $(wildcard wf_drv/*.c) misc/ls_efuse.c
endif
CSRCS += rf/DpdEst.c $(wildcard rf/rf*.c)
ifeq ($(CFG_DPD_LUNA), 1)
CSRCS += rf/DpdEstLuna.c
endif
CPPSRCS	=
SSRCS	=

#macro ctrl
ifeq ($(strip $(WCN_TYPE)), both)
CFLAGS = -DWCN_TYPE_WF -DWCN_TYPE_BT
else ifeq ($(strip $(WCN_TYPE)), wf)
CFLAGS = -DWCN_TYPE_WF
else ifeq ($(strip $(WCN_TYPE)), bt)
CFLAGS = -DWCN_TYPE_BT
endif

ifeq ($(RFCALI_WF_EN), 1)
CFLAGS += -DRFCALI_WF_EN
endif
ifeq ($(RFCALI_BT_EN), 1)
CFLAGS += -DRFCALI_BT_EN
endif

ifeq ($(CFG_DPD_LUNA), 1)
CFLAGS += -DDPD_LUNA=1
else
CFLAGS += -DDPD_LUNA=0
endif

ifeq ($(CFG_SS_CORNER), 1)
CFLAGS += -DSS_CORNER
else ifeq ($(CFG_FF_CORNER), 1)
CFLAGS += -DFF_CORNER
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

ifeq ($(WIFI_RAM_ATE), 1)
CFLAGS += -DWIFI_RAM_ATE
endif

RF_BOARD_VER ?= 0
CFLAGS += -DRF_BOARD_VER=$(RF_BOARD_VER)

#includings and flags
CFLAGS +=  -I $(TOPDIR)/include/CMSIS \
          -I $(TOPDIR)/include/CMSIS/core \
          -I $(TOPDIR)/chip/${CHIP}/include/ \
          -I $(TOPDIR)/chip/${CHIP}/include/wifi \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/include/ \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc/ \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/ext_rf/ \
          -I $(TOPDIR)/chip/${CHIP}/bsp/ \
          -I $(TOPDIR)/chip/${CHIP}/driver/spi \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/patch \
          -I $(TOPDIR)/src/${APP_PATH} \
          -I $(TOPDIR)/modules/nvs \
          -I $(TOPDIR)/modules/nvs/include \
          -I $(TOPDIR)/include/ \
          -I $(TOPDIR)/include/luna

ifneq ($(RFCALI_MODE), 0)
CFLAGS += -I $(TOPDIR)/bt/plf/api \
          -I $(TOPDIR)/bt/plf/${CHIP}/src/arch \
          -I $(TOPDIR)/bt/plf/${CHIP}/src/arch/compiler/gnuarm \
          -I $(TOPDIR)/bt/plf/${CHIP}/src/arch/ll/gnuarm \
          -I $(TOPDIR)/bt/ip/ble/hl/api \
          -I $(TOPDIR)/bt/modules/common/api \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc/ \
          -I $(TOPDIR)/bt/bt_task/api \
          -I $(TOPDIR)/bt/ip/dm/btc/api \
          -I $(TOPDIR)/bt/ip/ble/ll/api
endif

include $(TOPDIR)/rules.mk

