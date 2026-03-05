TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= wifi_ble_coex

export APP_PATH=arcs/wifi_ble_coex

SHRINK ?= 0

include default.config
include $(TOPDIR)/src/${CHIP}/general.mk

MODULES +=  profiles  #bsp driver debug wcnd patch nvs std

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS    +=  -lprf  -lbtos#-lbsp -lpatch -lnvs -lwcnd  -lstd


ifeq ($(INNER), 0)
EXTLIBS += -L./lib -lble -lbt_base -lstack  #-lbt
else
LIBS    += -lstack #-lble -lbt_base  -lbt
MODULES += bt
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CSRCS	+= 	$(wildcard src/*.c)
CSRCS	+= 	$(wildcard pm/*.c)
CSRCS	+= 	$(wildcard if/bt_if/*.c)
CSRCS	+= 	$(wildcard task/*.c)
CPPSRCS	=
SSRCS	= $(wildcard *.S)

#includings and flags

#CFLAGS += -I $(TOPDIR)/chip/${CHIP}/cli_cmd
CFLAGS += -I $(TOPDIR)/modules/profiles/ota/otas/api  \
		-I $(TOPDIR)/modules/profiles/hogp/hogpd/api \
		-I $(TOPDIR)/modules/profiles/bas/bass/api  \
		-I $(TOPDIR)/modules/profiles/dis/diss/api \
		-I $(TOPDIR)/modules/profiles/netcfg_ble/netcfg_bles/api  \
		-I $(TOPDIR)/modules/profiles/hogp \
		-I $(TOPDIR)/wifi/macsw/ip/lmac/src/rwnx \
		-I $(TOPDIR)/wifi/macsw/modules/common/src \
		-I $(TOPDIR)/wifi/macsw/modules/rtos/src \
		-I $(TOPDIR)/wifi/macsw/plf/refip/src/driver \
		-I $(TOPDIR)/chip/${CHIP}/btos \
		-I $(TOPDIR)/chip/${CHIP}/btos/task/api \
		-I $(TOPDIR)/chip/${CHIP}/atcmd \
		-I $(TOPDIR)/chip/${CHIP}/include/bt \
		-I ./api		\
		-I ./task/api	\
		-I ./if/bt_if	\
		-I ./pm		\
		-I ./src		\
		-I .

# copy cflags from RW IP
#CFLAGS += -DCFG_STATIC

#OPTMIZED_SIZE
BUILD_NANO=1
#Support PSRAM section
PSRAM_SEC=0
SYS_TRACE ?= 0

SYS_PSM=1
ifneq ("$(SHRINK)", "1")
CFG_BACK_TRACE := 1
endif

# wifi memory optimization
CFG_MEM_OPT ?= on
export CFG_MEM_OPT

export SHRINK
export SYS_TRACE SYS_PSM CFG_BACK_TRACE
ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
endif
ifeq ($(SHRINK), 1)
CFLAGS += -DSHRINK=1
endif

CFG_CORNER=tt
export CFG_CORNER
ifeq ($(SS_CORNER), 1)
CFG_SS_CORNER=1
CFG_CORNER=ss
export CFG_SS_CORNER
#export CFG_CORNER
else ifeq ($(FF_CORNER), 1)
CFG_FF_CORNER=1
CFG_CORNER=ff
export CFG_FF_CORNER
#export CFG_CORNER
endif

WIFI_API=on
export WIFI_API
ifeq ($(WIFI_API), on)
CFLAGS += -DWIFI_API=1
endif

CFG_RTOS=1
export CFG_RTOS
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

ifeq ($(SYS_PSM), 1)
CFLAGS += -DSYS_PSM
endif

TX_BUF_COPY ?= 0
ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif
export TX_BUF_COPY

HW_VER=HW_VER_ARCS_D0
export HW_VER

BT_WIFI_COEX=1
export BT_WIFI_COEX
CFLAGS += -DBT_WIFI_COEX=$(BT_WIFI_COEX)

PROFILE_NETCFG=1
export PROFILE_NETCFG
CFLAGS += -DPROFILE_NETCFG=$(PROFILE_NETCFG)

# Note: WCN_TYPE=wf:WiFi only, WCN_TYPE=bt:BT only, WCN_TYPE=both:Both support for Wifi and BT
WCN_TYPE=both
export WCN_TYPE
# RF_BOARD_VER=0 or undefine for ArcsD EVB,
# RF_BOARD_VER=1 for Taoyun DVT1
# RF_BOARD_VER=2 for Taoyun DVT2
# RF_BOARD_VER=3 for Haier
RF_BOARD_VER ?= 0
export RF_BOARD_VER
# RFCALI_WF_EN: 0=disable, 1=enable
RFCALI_WF_EN=1
export RFCALI_WF_EN
CFLAGS += -DRFCALI_WF_EN=$(RFCALI_WF_EN)
# RFCALI_BT_EN: 0=disable, 1=enable
RFCALI_BT_EN=1
export RFCALI_BT_EN
CFLAGS += -DRFCALI_BT_EN=$(RFCALI_BT_EN)
CFG_DPD_LUNA=0
export CFG_DPD_LUNA
CFG_RF_SELFCALI_FROM_NV=1
export CFG_RF_SELFCALI_FROM_NV
CFG_RF_SELFCALI_WRITE_TO_NV=0
export CFG_RF_SELFCALI_WRITE_TO_NV
CFG_FLASH_IF=1
export CFG_FLASH_IF
CFG_USE_FLASH_OTP=0
export CFG_USE_FLASH_OTP

CFG_PING=1
CFG_IPERF=1
ifeq ($(CFG_PING), 1)
CFLAGS += -DCFG_PING
export CFG_PING
endif
ifeq ($(CFG_IPERF), 1)
CFLAGS += -DCFG_IPERF
export CFG_IPERF
endif
CFG_WIFI_MFG=1
ifeq ($(CFG_WIFI_MFG), 1)
CFLAGS += -DCFG_WIFI_MFG
export CFG_WIFI_MFG
endif

CFG_MEMDUMP=1
ifeq ($(strip $(CFG_MEMDUMP)), 1)
CFLAGS += -DCFG_MEMDUMP
export CFG_MEMDUMP
endif

CONFIG_EXT_RAM=1
ifeq ($(strip $(CONFIG_EXT_RAM)), 1)
CFLAGS += -DCONFIG_EXT_RAM
export CONFIG_EXT_RAM
endif
CONFIG_RTOS_AL = 1
export CONFIG_RTOS_AL

CFG_NVS=1
export CFG_NVS

export BUILD_DATE := $(shell date "+0x%Y%m%d")
export CHANGE_ID_log := $(strip $(shell  git log -1 --format=%H))
export BT_CHANGE_ID_log :=$(strip $(shell cd $(TOPDIR)/bt &&git log -1 --format=%H && cd ../src/$(CHIP)/$(TGT)))
#$(warning ********changeid is $(CHANGE_ID_log) btchangeid is $(BT_CHANGE_ID_log) )
ifeq ($(BT_CHANGE_ID_log),)
    BT_CHANGE_ID = 0XFFFF
else
    BT_CHANGE_ID = 0x$(shell  echo $(BT_CHANGE_ID_log) | cut -c 1-8)
endif
ifeq ($(CHANGE_ID_log),)
    CHANGE_ID = 0XFFFF
else
    CHANGE_ID = 0x$(shell  echo $(CHANGE_ID_log) | cut -c 1-8)
endif

CFLAGS += -DBUILD_DATE=${BUILD_DATE}
CFLAGS += -DCHANGE_ID=${CHANGE_ID}
CFLAGS += -DBT_CHANGE_ID=${BT_CHANGE_ID}

ifeq ($(IC_BOARD), 1)
ifeq ($(CONFIG_HARTID),1)
ext:
	@sh "$(TOPDIR)/tools/mkhdr.sh" $(TGTOUT)/$(TARGET).bin
	#./merge_cp.sh $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000
ifeq ("$(CFG_AMP_IPC)", "1")
	python ./merge_cp.py $(TGTOUT)/ap_host.bin $(TGTOUT)/$(TARGET).bin 0x00100000 $(TGTOUT)/$(TARGET)_cp.bin
else
	python ./merge_cp.py $(TOPDIR)/chip/$(CHIP)/cp_load_bin/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000 $(TGTOUT)/$(TARGET)_cp.bin
endif
else
ext:
	@sh "$(TOPDIR)/tools/mkhdr.sh" $(TGTOUT)/$(TARGET).bin
endif
endif

include $(TOPDIR)/rules.mk
#LDSCRIPT  = ./flash.ld
ifeq ($(PSRAM_SEC),1)
LDSCRIPT   = $(TOPDIR)/chip/${CHIP}/arch/psram.ld
else
ifeq ($(IC_BOARD), 1)
ifeq ($(CONFIG_HARTID),1)
ifeq ($(SHRINK),1)
#LDSCRIPT   = ./dp_sram_cp.ld
LDSCRIPT   = ./shrink_cp.ld
else
ifeq ("$(CFG_AMP_IPC)", "1")
LDSCRIPT   = ./flash_cp_ipc.ld
else
LDSCRIPT   = ./flash_cp.ld
endif
endif
else
LDSCRIPT   = ./flash.ld
endif
#LDSCRIPT   = ./fpga_core1.ld
else
LDSCRIPT   = ./fpga_core1.ld
endif
endif
