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
ifneq ($(filter both both_dual, $(strip $(WCN_TYPE))),)
CSRCS	= $(wildcard *.c) $(wildcard sleep/*.c)  $(wildcard wf_drv/*.c) $(wildcard ext_rf/*.c) $(wildcard bt_drv/*.c) $(wildcard timer/*.c) $(wildcard dma/*.c) $(wildcard nv/*.c) $(wildcard misc/*.c)
else ifeq ($(strip $(WCN_TYPE)), wf)
CSRCS	= $(wildcard *.c)  $(wildcard wf_drv/*.c) $(wildcard nv/*.c) $(wildcard misc/*.c)
else ifeq ($(strip $(WCN_TYPE)), bt)
CSRCS	= $(wildcard *.c) $(wildcard sleep/*.c)  $(wildcard ext_rf/*.c) $(wildcard bt_drv/*.c) $(wildcard timer/*.c) $(wildcard dma/*.c) $(wildcard nv/*.c) $(wildcard wf_drv/*.c) $(wildcard misc/*.c)
endif
CSRCS += rf/DpdEst.c $(wildcard rf/rf*.c)
ifeq ($(CFG_DPD_LUNA), 1)
CSRCS += rf/DpdEstLuna.c
endif
CPPSRCS	=
SSRCS	=

#macro ctrl
ifeq ($(strip $(WCN_TYPE)), both_dual)
CFLAGS = -DWCN_TYPE_WF -DWCN_TYPE_BT -DWCN_TYPE_BT_DUAL
else ifeq ($(strip $(WCN_TYPE)), both)
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
CFG_MFG_RF_TEST ?= 0
ifeq ($(CFG_MFG_RF_TEST), 1)
CFLAGS += -DMFG_RF_TEST
endif
ifeq ($(CFG_USE_FLASH_OTP), 1)
CFLAGS += -DUSE_FLASH_OTP=1
endif

ifeq ($(WIFI_RAM_ATE), 1)
CFLAGS += -DWIFI_RAM_ATE
endif

RF_BOARD_VER ?= 0
CFLAGS += -DRF_BOARD_VER=$(RF_BOARD_VER)

CFG_FLASH_NOR_OTP_NV_BASE_ADDR ?= 0x30200000
CFLAGS += -DFLASH_NOR_OTP_NV_BASE_ADDR=$(CFG_FLASH_NOR_OTP_NV_BASE_ADDR)

# Golden value configure:
# Golden value set flag
CFG_WIFI_RF_SET_GOLDEN ?= 0
CFLAGS += -DWIFI_RF_SET_GOLDEN=$(CFG_WIFI_RF_SET_GOLDEN)
# xo cap golden value
CFG_XO_CAP_GOLDEN_VAL ?= 0
CFLAGS += -DXO_CAP_GOLDEN_VAL=$(CFG_XO_CAP_GOLDEN_VAL)
# low channel power offset golden value
CFG_PWR_OFFSET_LOW_GOLDEN_VAL ?= 0
CFLAGS += -DPWR_OFFSET_LOW_GOLDEN_VAL=$(CFG_PWR_OFFSET_LOW_GOLDEN_VAL)
# Middle channel power offset golden value
CFG_PWR_OFFSET_MID_GOLDEN_VAL ?= 0
CFLAGS += -DPWR_OFFSET_MID_GOLDEN_VAL=$(CFG_PWR_OFFSET_MID_GOLDEN_VAL)
# High channel power offset golden value
CFG_PWR_OFFSET_HIGH_GOLDEN_VAL ?= 0
CFLAGS += -DPWR_OFFSET_HIGH_GOLDEN_VAL=$(CFG_PWR_OFFSET_HIGH_GOLDEN_VAL)
# DSSS RSSI offset golden value
CFG_RSSI_OFFSET_DSSS_GOLDEN_VAL ?= 0
CFLAGS += -DRSSI_OFFSET_DSSS_GOLDEN_VAL=$(CFG_RSSI_OFFSET_DSSS_GOLDEN_VAL)
# OFDM RSSI offset golden value
CFG_RSSI_OFFSET_OFDM_GOLDEN_VAL ?= 0
CFLAGS += -DRSSI_OFFSET_OFDM_GOLDEN_VAL=$(CFG_RSSI_OFFSET_OFDM_GOLDEN_VAL)

# Factory RF parameter partition base address in flash.
# The data may include: mac address, xo_cap, power offset, rssi offset, power table, ppa gain...
# The reserved partition size should not be less than 512 bytes.
CFG_FLASH_WF_MFG_CONF_BASE_ADDR ?= 0
CFLAGS += -DFLASH_WF_MFG_CONF_BASE_ADDR=$(CFG_FLASH_WF_MFG_CONF_BASE_ADDR)

# Unit test control
CFG_NV_EFUSE_UNIT_TEST ?= 0
ifeq ($(CFG_NV_EFUSE_UNIT_TEST), 1)
CFLAGS += -DCFG_NV_EFUSE_UNIT_TEST
endif

#includings and flags
CFLAGS +=  -I $(TOPDIR)/include/CMSIS \
          -I $(TOPDIR)/include/CMSIS/core \
          -I $(TOPDIR)/chip/${CHIP}/include/ \
          -I $(TOPDIR)/chip/${CHIP}/include/wifi \
          -I $(TOPDIR)/chip/${CHIP}/include/bt \
          -I $(TOPDIR)/chip/${CHIP}/include/ipc \
          -I $(TOPDIR)/chip/${CHIP}/include/utils \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/include/ \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc/ \
          -I $(TOPDIR)/chip/${CHIP}/wcnd/ext_rf/ \
          -I $(TOPDIR)/chip/${CHIP}/btos \
          -I $(TOPDIR)/chip/${CHIP}/bsp/ \
          -I $(TOPDIR)/chip/${CHIP}/driver/spi \
          -I $(TOPDIR)/chip/${CHIP}/atcmd/ \
          -I $(TOPDIR)/chip/${CHIP}/bt_hal/ \
          -I $(TOPDIR)/chip/${CHIP}/cli_cmd/ \
          -I $(TOPDIR)/chip/${CHIP}/btos/ \
          -I $(TOPDIR)/chip/${CHIP}/btos/task/api/ \
          -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/patch \
          -I $(TOPDIR)/src/${APP_PATH} \
          -I $(TOPDIR)/modules/nvs \
          -I $(TOPDIR)/modules/nvs/include \
          -I $(TOPDIR)/modules/rtos/include \
          -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
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

## TODO in the future may need use -O3 for timecost
##FILTERED_CFLAGS := $(filter-out -O%,$(CFLAGS))
##CFLAGS := $(FILTERED_CFLAGS) -O3
