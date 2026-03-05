TOPDIR=../../..

export LWIP_VER := lwip-2.2.0
#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= cp_demo

export APP_PATH=arcs/cp_demo

SHRINK ?= 0

include default.config
include $(TOPDIR)/src/${CHIP}/general.mk

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	= $(wildcard *.S)

#includings and flags

CFLAGS += -I $(TOPDIR)/chip/${CHIP}/cli_cmd \
		 -I $(TOPDIR)/src/${APP_PATH} 		\
		 -I $(TOPDIR)/src/${AP_TARGET} 		\
		 -I $(TOPDIR)/modules/nvs/include	\
		 -I $(TOPDIR)/modules/ota/include	\
		 -I $(TOPDIR)/modules/rtos/include \
		 -I $(TOPDIR)/chip/${CHIP}/include/ipc \
		 -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_core \
		 -I $(TOPDIR)/chip/$(CHIP)/ipc/ipc_utils \
		 -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_slave \
		 -I $(TOPDIR)/chip/${CHIP}/ic_lock


unexport CONFIG_FREERTOSCONFIG_H_PATH

export BUILD_DATE := $(shell date "+0x%Y%m%d")
export BUILD_NANO := 1
export CONFIG_HARTID = 1
export CONFIG_SKIP_BOOTCLOCK=1

CFLAGS += -DBUILD_DATE=${BUILD_DATE}

#EXTLIBS += -L$(TOPDIR)/chip/$(CHIP)/lib

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

CONFIG_HARTID=1
export CONFIG_HARTID

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

# Note: WCN_TYPE=wf:WiFi only, WCN_TYPE=bt:BT only, WCN_TYPE=both:Both support for Wifi and BT
WCN_TYPE=wf
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
RFCALI_BT_EN=0
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

CFG_MEMDUMP=0
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

CFG_FLASH_IF := 1
export CFG_FLASH_IF

CFG_AMP_IPC_FLASH_AGENT = 1
export CFG_AMP_IPC_FLASH_AGENT

CFG_AMP_IPC_HALT_PEER_CORE = 1
export CFG_AMP_IPC_HALT_PEER_CORE

CFG_AMP_IPC_SLAVE := 1
export CFG_AMP_IPC_SLAVE

CFG_AMP_IPC_MRPC_SERVER := 1
CFG_AMP_IPC_MRPC_SERVER_FLASH_IF := 1
export CFG_AMP_IPC_MRPC_SERVER CFG_AMP_IPC_MRPC_SERVER_FLASH_IF

ifeq ($(strip $(CFG_AMP_IPC_MRPC_SERVER_FLASH_IF)), 1)
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/ipc/mrpc/flash_if
endif
#ext1 : # don't do this except TapeOut
#	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
#	mkdir -p $(TOPDIR)/chip/$(CHIP)/fhost
#	cp $(TGTOUT)/$(TARGET).bin $(TOPDIR)/chip/$(CHIP)/fhost/
#	cp $(TGTOUT)/$(TARGET).dis $(TOPDIR)/chip/$(CHIP)/fhost/
#	cp $(TGTOUT)/$(TARGET).readelf $(TOPDIR)/chip/$(CHIP)/fhost/
#	cp $(TGTOUT)/$(TARGET).symbol $(TOPDIR)/chip/$(CHIP)/fhost/
#	cp $(TGTOUT)/$(TARGET) $(TOPDIR)/chip/$(CHIP)/fhost/
#	$(READELF) -p .patch.entries $(TGTOUT)/$(TARGET) | grep "^ " | sed 's/.*\] *//' > $(TOPDIR)/chip/$(CHIP)/fhost/patch_funcs.lst
#	python $(TOPDIR)/modules/patch/py/sym_to_h.py $(TGTOUT)/$(TARGET).symbol $(TOPDIR)/chip/$(CHIP)/fhost/patch_funcs.lst $(TOPDIR)/chip/$(CHIP)/fhost/patch_def.h
#	python $(TOPDIR)/modules/patch/py/sym_to_ld.py $(TGTOUT)/$(TARGET).symbol  $(TOPDIR)/chip/$(CHIP)/fhost/patch_sym.ld

#ifeq ($(IC_BOARD), 1)
#ifeq ($(CONFIG_HARTID),1)
#ext:
#	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
#	#./merge_cp.sh $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000
#ifeq ("$(CFG_AMP_IPC)", "1")
#	python ./merge_cp.py $(TGTOUT)/ap_host.bin $(TGTOUT)/$(TARGET).bin 0x00100000 $(TGTOUT)/$(TARGET)_cp.bin
#else
#	python ./merge_cp.py $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000 $(TGTOUT)/$(TARGET)_cp.bin
#endif
#else
#ext:
#	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
#endif
#endif

include $(TOPDIR)/rules.mk
LDSCRIPT   = ./flash_cp.ld

ext:
	@sh ${TOPDIR}/tools/mkhdr_ota.sh $(TGTOUT)/$(TARGET).bin
	@export CONFIG_FREERTOSCONFIG_H_PATH="src/arcs/ap_demo"

#LDSCRIPT  = ./flash.ld
#ifeq ($(PSRAM_SEC),1)
#LDSCRIPT   = $(TOPDIR)/chip/${CHIP}/arch/psram.ld
#else
#ifeq ($(IC_BOARD), 1)
#ifeq ($(CONFIG_HARTID),1)
#ifeq ($(SHRINK),1)
##LDSCRIPT   = ./dp_sram_cp.ld
#LDSCRIPT   = ./shrink_cp.ld
#else
#ifeq ("$(CFG_AMP_IPC)", "1")
#LDSCRIPT   = ./flash_cp_ipc.ld
#else
#LDSCRIPT   = ./flash_cp.ld
#endif
#endif
#else
#LDSCRIPT   = ./flash.ld
#endif
##LDSCRIPT   = ./fpga_core1.ld
#else
#LDSCRIPT   = ./fpga_core1.ld
#endif
#endif
