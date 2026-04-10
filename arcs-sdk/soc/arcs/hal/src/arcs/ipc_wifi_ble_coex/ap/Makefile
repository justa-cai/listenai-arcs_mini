TOPDIR=../../../../

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= ipc_ap_wifi_ble_coex

export APP_PATH=arcs/ipc_wifi_ble_coex/ap

SHRINK ?= 0

BT_MODE = ble_single_mode
# modules depend by this module
include default.config
include $(TOPDIR)/src/${CHIP}/general.mk

MODULES +=  profiles  bt_hal #bsp driver debug wcnd patch nvs std

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	+=  -lprf  -lbtos -lbt_hal #-lbsp -lpatch -lnvs -lwcnd  -lstd


ifeq ($(INNER), 0)
#EXTLIBS +=  -lble -lbt_base -lstack  #-lbt   -L.$(TOPDIR)/chip/${CHIP}/lib/$(BT_MODE)
else
LIBS    += -lstack #-lble -lbt_base  -lbt
MODULES += bt
endif
PERF ?= 0
PSRAM_SEC ?=0

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
CFLAGS += -I $(TOPDIR)/include/NMSIS \
         -I $(TOPDIR)/include/NMSIS/core \
         -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
         -I $(TOPDIR)/chip/${CHIP}/cli_cmd \
         -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
         -I $(TOPDIR)/chip/${CHIP}/atcmd \
         -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_core \
         -I $(TOPDIR)/chip/$(CHIP)/ipc/ipc_utils \
         -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_slave \
         -I $(TOPDIR)/chip/${CHIP}/include/ipc \
         -I $(TOPDIR)/modules/rtos/include \
         -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
         -I $(TOPDIR)/modules/profiles/ota/otas/api  \
         -I $(TOPDIR)/modules/profiles/hogp/hogpd/api \
         -I $(TOPDIR)/modules/profiles/bas/bass/api  \
         -I $(TOPDIR)/modules/profiles/dis/diss/api \
         -I $(TOPDIR)/modules/profiles/netcfg_ble/netcfg_bles/api  \
         -I $(TOPDIR)/modules/profiles/hogp \
         -I $(TOPDIR)/modules/profiles/netcfg_ble/netcfg_bles/api  \
         -I $(TOPDIR)/chip/${CHIP}/btos \
         -I $(TOPDIR)/chip/${CHIP}/btos/task/api \
         -I $(TOPDIR)/chip/${CHIP}/ic_lock \
         -I $(TOPDIR)/chip/${CHIP}/include/bt \
		-I $(TOPDIR)/chip/${CHIP}/bt_hal \
         -I ./api		\
         -I ./task/api	\
         -I ./if/bt_if	\
         -I ./pm		\
         -I ./src		\
         -I .

# copy cflags from RW IP
#CFLAGS += -DCFG_STATIC  #-DCFG_ROM

CONFIG_HARTID=0
export CONFIG_HARTID

#BUILD_ROM=1

#OPTMIZED_SIZE
BUILD_NANO=1




#HW_VER=d0
HW_VER=HW_VER_ARCS_D0
export HW_VER
ifeq ($(IC_BOARD), 1)
  ifeq ("$(strip $(CHIP))", "arcs")
    ifeq ("$(strip $(HW_VER))", "HW_VER_ARCS_C0")
      CFLAGS += -DCHIP_ARCS_C0
    else ifeq ("$(strip $(HW_VER))", "HW_VER_ARCS_D0")
      CFLAGS += -DCHIP_ARCS_D0
    endif
  endif
endif

BT_WIFI_COEX=1
export BT_WIFI_COEX
CFLAGS += -DBT_WIFI_COEX=$(BT_WIFI_COEX)
# Note: WCN_TYPE=wf:WiFi only, WCN_TYPE=bt:BT only, WCN_TYPE=both:Both support for Wifi and BT
WCN_TYPE=both
export WCN_TYPE




CFG_RTOS=1
export CFG_RTOS
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

CONFIG_RTOS_AL = 1
export CONFIG_RTOS_AL

CFG_AMP_IPC_SLAVE := 1
CFG_AMP_IPC_WIFI_CHAN := 1
CFG_AMP_IPC_TCPIP := 1
CFG_AMP_IPC_HALT_BY_PEER_CORE := 1
CFG_AMP_IPC_HALT_PEER_CORE := 1
CFG_AMP_IPC_MRPC_SERVER := 1
CFG_AMP_IPC_MRPC_CLIENT := 1
CFG_AMP_IPC_MRPC_CLIENT_LWIP := 1
CFG_AMP_IPC_MRPC_SERVER_WIFI := 1
CFG_AMP_IPC_MRPC_CLIENT_NVS  := 1
CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF := 1
CFG_AMP_IPC_MRPC_SERVER_BT := 1

export CFG_AMP_IPC_SLAVE CFG_AMP_IPC_MRPC_SERVER CFG_AMP_IPC_MRPC_CLIENT CFG_AMP_IPC_MRPC_SERVER_BT
export CFG_AMP_IPC_MRPC_CLIENT_LWIP CFG_AMP_IPC_MRPC_SERVER_WIFI CFG_AMP_IPC_MRPC_CLIENT_NVS
export CFG_FLASH_IPC CFG_AMP_IPC_WIFI_CHAN CFG_AMP_IPC_HALT_BY_PEER_CORE CFG_AMP_IPC_TCPIP CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF CFG_AMP_IPC_HALT_PEER_CORE

# mrpc client: GPADC get temp .. on wifi side
CFG_AMP_IPC_MRPC_CLIENT_UTILS_S2M  := 1
export  CFG_AMP_IPC_MRPC_CLIENT_UTILS_S2M

CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S  := 1
export  CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S


SYS_TRACE ?= 0

ifeq ($(IC_BOARD), 1)
SYS_PSM=1
endif
# wifi memory optimization
CFG_MEM_OPT ?= on
export CFG_MEM_OPT
export SHRINK
export SYS_TRACE SYS_PSM
ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
endif


ifeq ($(SHRINK), 1)
CFLAGS += -DSHRINK=1
endif
#cali use independent buf or not
#CALI_BUF=0
#export CALI_BUF
#ifneq ($(CALI_BUF),)
#CFLAGS += -DCALI_BUF
#endif

WIFI_API=on
export WIFI_API
ifeq ($(WIFI_API), on)
CFLAGS += -DWIFI_API=1
endif




ifeq ($(SYS_PSM), 1)
CFLAGS += -DSYS_PSM
endif




RF_BOARD_VER=0
export RF_BOARD_VER

# RFCALI_WF_EN: 0=disable, 1=enable
RFCALI_WF_EN=1
export RFCALI_WF_EN
CFLAGS += -DRFCALI_WF_EN=$(RFCALI_WF_EN)
# RFCALI_BT_EN: 0=disable, 1=enable
RFCALI_BT_EN=1
export RFCALI_BT_EN
CFLAGS += -DRFCALI_BT_EN=$(RFCALI_BT_EN)

CFG_RF_SELFCALI_FROM_NV=0
export CFG_RF_SELFCALI_FROM_NV
CFG_RF_SELFCALI_WRITE_TO_NV=0
export CFG_RF_SELFCALI_WRITE_TO_NV
CFG_FLASH_IF=0
export CFG_FLASH_IF
CFG_USE_FLASH_OTP=0
export CFG_USE_FLASH_OTP


PROFILE_NETCFG=1
export PROFILE_NETCFG
CFLAGS += -DPROFILE_NETCFG=$(PROFILE_NETCFG)




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

CFG_BACK_TRACE := 1
export CFG_BACK_TRACE

CFG_MEMDUMP := 0
ifeq ($(strip $(CFG_MEMDUMP)), 1)
export CFG_MEMDUMP
endif



ifeq ($(PSRAM_HEAP), 1)
CFLAGS += -DPSRAM_HEAP
endif




CONFIG_EXT_RAM=1
ifeq ($(strip $(CONFIG_EXT_RAM)), 1)
CFLAGS += -DCONFIG_EXT_RAM
export CONFIG_EXT_RAM
endif

ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif

CFG_CRYPTO_IC_MUTEX = 1
ifeq ("$(strip $(CFG_CRYPTO_IC_MUTEX))","1")
CFLAGS  += -DCFG_CRYPTO_IC_MUTEX
endif

ifeq ("$(strip $(CONFIG_PM))","1")
CFG_AMP_IPC_MRPC_CLIENT_NVS = 0
CFG_TOTAL_HEAP_SIZE = 40960
CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF := 0
endif

ext1 : # don't do this except TapeOut
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
	mkdir -p $(TOPDIR)/chip/$(CHIP)/fhost
	cp $(TGTOUT)/$(TARGET).bin $(TOPDIR)/chip/$(CHIP)/fhost/
	cp $(TGTOUT)/$(TARGET).dis $(TOPDIR)/chip/$(CHIP)/fhost/
	cp $(TGTOUT)/$(TARGET).readelf $(TOPDIR)/chip/$(CHIP)/fhost/
	cp $(TGTOUT)/$(TARGET).symbol $(TOPDIR)/chip/$(CHIP)/fhost/
	cp $(TGTOUT)/$(TARGET) $(TOPDIR)/chip/$(CHIP)/fhost/
	$(READELF) -p .patch.entries $(TGTOUT)/$(TARGET) | grep "^ " | sed 's/.*\] *//' > $(TOPDIR)/chip/$(CHIP)/fhost/patch_funcs.lst
	python $(TOPDIR)/modules/patch/py/sym_to_h.py $(TGTOUT)/$(TARGET).symbol $(TOPDIR)/chip/$(CHIP)/fhost/patch_funcs.lst $(TOPDIR)/chip/$(CHIP)/fhost/patch_def.h
	python $(TOPDIR)/modules/patch/py/sym_to_ld.py $(TGTOUT)/$(TARGET).symbol  $(TOPDIR)/chip/$(CHIP)/fhost/patch_sym.ld

ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin

include $(TOPDIR)/rules.mk

ifeq ($(IC_BOARD), 1)
ifeq ("$(strip $(CONFIG_PM))","1")
LDSCRIPT   = ./flash_pm.ld
else
LDSCRIPT   = ./flash.ld
endif
else
LDSCRIPT   = ./fpga_core1.ld
endif

