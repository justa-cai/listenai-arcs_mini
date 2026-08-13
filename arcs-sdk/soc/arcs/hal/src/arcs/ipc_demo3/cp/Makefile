TOPDIR=../../../../

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= cp_host

export APP_PATH=arcs/ipc_demo3/cp

SHRINK ?= 0
#export SHRINK
include default.config
include $(TOPDIR)/src/${CHIP}/general.mk


#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS += -I $(TOPDIR)/include/NMSIS/Core/Include \
         -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
         -I $(TOPDIR)/chip/${CHIP}/include/ipc \
         -I $(TOPDIR)/chip/${CHIP}/atcmd \
         -I $(TOPDIR)/chip/${CHIP}/cli_cmd \
         -I $(TOPDIR)/chip/${CHIP}/event \
         -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include\
         -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
         -I $(TOPDIR)/chip/${CHIP}/ic_lock


# copy cflags from RW IP
CFLAGS += -DCFG_STATIC  -DCFG_ROM

#BUILD_ROM=1

#OPTMIZED_SIZE
BUILD_NANO=1
SYS_TRACE ?= 0
ifeq ($(IC_BOARD), 1)
SYS_PSM=1
endif
#Support PSRAM section
PSRAM_SEC=0
export PSRAM_SEC
#SYS_TRACE=1
#export SYS_TRACE
# wifi memory optimization
CFG_MEM_OPT ?= on
export CFG_MEM_OPT
export SYS_TRACE SYS_PSM
ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
endif
ifeq ($(SHRINK), 1)
CFLAGS += -DSHRINK=1
endif

WIFI_API=on
export WIFI_API
ifeq ($(WIFI_API), on)
CFLAGS += -DWIFI_API=1
endif

HW_VER=HW_VER_ARCS_D0
export HW_VER

#cali use independent buf or not
#CALI_BUF=0
#export CALI_BUF
#ifneq ($(CALI_BUF),)
#CFLAGS += -DCALI_BUF
#endif
CFG_RF_SELFCALI_FROM_NV=0
export CFG_RF_SELFCALI_FROM_NV
CFG_RF_SELFCALI_WRITE_TO_NV=0
export CFG_RF_SELFCALI_WRITE_TO_NV
CFG_FLASH_IF=1
export CFG_FLASH_IF
CFG_USE_FLASH_OTP=0
export CFG_USE_FLASH_OTP

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


ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
else
endif

CFG_RTOS := 1
CONFIG_RTOS_AL := 1
CFG_IPERF := 1
CFG_PING := 1
CFG_BACK_TRACE := 1
CFG_AMP_IPC_SLAVE := 1

CFG_AMP_IPC_MRPC_SERVER := 1
CFG_AMP_IPC_MRPC_CLIENT := 1
CFG_AMP_IPC_MRPC_CLIENT_LUNA := 1
CFG_AMP_IPC_HALT_PEER_CORE := 1
CFG_AMP_IPC_HALT_BY_PEER_CORE := 1
CONFIG_SKIP_BOOTCLOCK = 1
CFG_AMP_IPC_INDICATION := 1
CFG_WIFI_MFG := 1
CFG_FLASH_IF := 1
export CFG_RTOS CONFIG_RTOS_AL CFG_IPERF CFG_PING CFG_BACK_TRACE CFG_AMP_IPC_SLAVE CONFIG_SKIP_BOOTCLOCK CFG_AMP_IPC_MRPC_SERVER
export CFG_AMP_IPC_MRPC_CLIENT CFG_AMP_IPC_MRPC_CLIENT_LUNA CFG_WIFI_MFG CFG_FLASH_IF
export CFG_AMP_IPC_HALT_PEER_CORE CFG_AMP_IPC_HALT_BY_PEER_CORE CFG_AMP_IPC_INDICATION

CFG_MEMDUMP := 0
ifeq ($(strip $(CFG_MEMDUMP)), 1)
export CFG_MEMDUMP
endif

ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

ifeq ($(PSRAM_HEAP), 1)
CFLAGS += -DPSRAM_HEAP
endif

CONFIG_HARTID=1
export CONFIG_HARTID

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

CFG_NVS = 1
export CFG_NVS
CONFIG_NVS_FLASH_XIP = 1
export CONFIG_NVS_FLASH_XIP

ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif

CFG_CRYPTO_IC_MUTEX = 1
ifeq ("$(strip $(CFG_CRYPTO_IC_MUTEX))","1")
CFLAGS  += -DCFG_CRYPTO_IC_MUTEX
endif

ifeq ("$(strip $(CONFIG_PM))","1")
CFG_AMP_IPC_MRPC_SERVER_NVS = 0
CFG_NVS = 0
endif

ext1 : # don't do this except TapeOut
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
	mkdir -p $(TOPDIR)/chip/$(CHIP)/ap_host
	cp $(TGTOUT)/$(TARGET).bin $(TOPDIR)/chip/$(CHIP)/ap_host/
	cp $(TGTOUT)/$(TARGET).dis $(TOPDIR)/chip/$(CHIP)/ap_host/
	cp $(TGTOUT)/$(TARGET).readelf $(TOPDIR)/chip/$(CHIP)/ap_host/
	cp $(TGTOUT)/$(TARGET).symbol $(TOPDIR)/chip/$(CHIP)/ap_host/
	cp $(TGTOUT)/$(TARGET) $(TOPDIR)/chip/$(CHIP)/ap_host/
	$(READELF) -p .patch.entries $(TGTOUT)/$(TARGET) | grep "^ " | sed 's/.*\] *//' > $(TOPDIR)/chip/$(CHIP)/ap_host/patch_funcs.lst
	python $(TOPDIR)/modules/patch/py/sym_to_h.py $(TGTOUT)/$(TARGET).symbol $(TOPDIR)/chip/$(CHIP)/ap_host/patch_funcs.lst $(TOPDIR)/chip/$(CHIP)/ap_host/patch_def.h
	python $(TOPDIR)/modules/patch/py/sym_to_ld.py $(TGTOUT)/$(TARGET).symbol  $(TOPDIR)/chip/$(CHIP)/ap_host/patch_sym.ld

ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
	#./merge_cp.sh $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000
	python ./merge_cp.py $(TGTOUT)/fhost.bin $(TGTOUT)/$(TARGET).bin 0x00010000 $(TGTOUT)/fhost_cp.bin

include $(TOPDIR)/rules.mk
ifeq ("$(strip $(CONFIG_PM))","1")
LDSCRIPT   = ./flash_pm.ld
else
LDSCRIPT   = ./flash.ld
endif
