TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= fhost

export APP_PATH=arcs/cli_demo

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

CFLAGS += -I $(TOPDIR)/chip/${CHIP}/cli_cmd

# copy cflags from RW IP
#CFLAGS += -DCFG_STATIC

#OPTMIZED_SIZE
BUILD_NANO=1
#Support PSRAM section

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

PERF ?= 0
PSRAM_SEC ?= 0
ifeq ($(PERF), 1)
CFLAGS += -DPERF=1
export PERF
endif
ifeq ($(PSRAM_SEC), 1)
CFLAGS += -DPSRAM_SEC=1
export PSRAM_SEC
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
CFG_RF_SELFCALI_FROM_NV=0
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

CONFIG_RAM_CONSTRAINED=1
ifeq ($(strip $(CONFIG_RAM_CONSTRAINED)), 1)
CFLAGS += -DCONFIG_RAM_CONSTRAINED
export CONFIG_RAM_CONSTRAINED
endif

CONFIG_RTOS_AL = 1
export CONFIG_RTOS_AL

CFG_NVS=1
export CFG_NVS

CONFIG_NVS_FLASH_XIP = 1
export CONFIG_NVS_FLASH_XIP

CONFIG_PM := 0
export CONFIG_PM

CONFIG_PM_UART_WAKEUP := 0

ifeq ($(CONFIG_PM), 1)

ifneq ($(CONFIG_PM_UART_WAKEUP), 1)
CONFIG_PM_CLOSE_AP := 1
export CONFIG_PM_CLOSE_AP
endif

export CONFIG_PM_UART_WAKEUP

CONFIG_PM_KEEP_ALIVE := 1
export CONFIG_PM_KEEP_ALIVE

CONFIG_PM_DEBUG := 1
export CONFIG_PM_DEBUG
endif
#cali use independent buf or not
#CALI_BUF=0
#export CALI_BUF
#ifneq ($(CALI_BUF),)
#CFLAGS += -DCALI_BUF
#endif

TX_BUF_COPY ?= 0
ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif
export TX_BUF_COPY

include $(TOPDIR)/src/${CHIP}/general_pre.mk

ifeq ($(IC_BOARD), 1)
ifeq ($(CONFIG_HARTID),1)
ext:
	@echo "// Generated file, do not edit." > $(SDK_HEADER_FILE)
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
	#./merge_cp.sh $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000
ifeq ("$(CFG_AMP_IPC)", "1")
	python ./merge_cp.py $(TGTOUT)/ap_host.bin $(TGTOUT)/$(TARGET).bin 0x00100000 $(TGTOUT)/$(TARGET)_cp.bin
else
	python ./merge_cp.py $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000 $(TGTOUT)/$(TARGET)_cp.bin
endif
else
ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
endif
endif

include $(TOPDIR)/rules.mk
#LDSCRIPT  = ./flash.ld
ifeq ($(IC_BOARD), 1)
ifeq ($(CONFIG_HARTID),1)
ifeq ($(SHRINK),1)
#LDSCRIPT   = ./dp_sram_cp.ld
LDSCRIPT   = ./shrink_cp.ld
else
ifeq ("$(CFG_AMP_IPC)", "1")
LDSCRIPT   = ./flash_cp_ipc.ld
else
ifeq ($(CONFIG_PM),1)
LDSCRIPT   = ./flash_cp_pm.ld
else
LDSCRIPT   = ./flash_cp.ld
endif
endif
endif
else
LDSCRIPT   = ./flash_ap.ld
endif
#LDSCRIPT   = ./fpga_core1.ld
else
LDSCRIPT   = ./fpga_core1.ld
endif
