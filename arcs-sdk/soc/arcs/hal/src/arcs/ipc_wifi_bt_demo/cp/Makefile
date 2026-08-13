TOPDIR=../../../../

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= ipc_wifi_bt_demo_cp

export APP_PATH=arcs/ipc_wifi_bt_demo/cp



BT_MODE = ipc_bt_dual

include default.config
include $(TOPDIR)/src/${CHIP}/general.mk


MODULES +=  profiles bt_hal

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS    +=   -lprf -lbt_hal

ifeq ($(INNER), 0)
ifeq ($(HARD_FLOAT),1)
EXTLIBS += -lstack -lbt_music -lbt_call -lbt_base_cp -L$(TOPDIR)/chip/${CHIP}/lib/$(BT_MODE)/ilpf32
else
EXTLIBS += -lstack -lbt_music -lbt_call -lbt_base_cp -L$(TOPDIR)/chip/${CHIP}/lib/$(BT_MODE)/ilp32
endif
else
LIBS    += -lstack -lbt_music -lbt_call -lbt_base
MODULES += bt
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)

CSRCS	+= 	$(wildcard if/bt_if/*.c)
CSRCS	+= 	$(wildcard if/aud_if/*.c)
CSRCS	+= 	$(wildcard if/app_if/*.c)
CSRCS	+= 	$(wildcard task/*.c)

CPPSRCS	=
SSRCS	= $(wildcard *.S)

#CFLAGS += -I $(TOPDIR)/chip/${CHIP}/cli_cmd
CFLAGS += -I $(TOPDIR)/modules/profiles/ota/otas/api  \
		-I $(TOPDIR)/modules/profiles/hogp/hogpd/api \
		-I $(TOPDIR)/modules/profiles/bas/bass/api  \
		-I $(TOPDIR)/modules/profiles/dis/diss/api \
		-I $(TOPDIR)/modules/profiles/netcfg_ble/netcfg_bles/api  \
		-I $(TOPDIR)/modules/profiles/hogp \
		-I $(TOPDIR)/modules/codec/api \
		-I $(TOPDIR)/modules/audio/api \
		-I $(TOPDIR)/modules/rtos/include \
		-I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
		-I $(TOPDIR)/wifi/macsw/ip/lmac/src/rwnx \
		-I $(TOPDIR)/wifi/macsw/modules/common/src \
		-I $(TOPDIR)/wifi/macsw/modules/rtos/src \
		-I $(TOPDIR)/wifi/macsw/plf/refip/src/driver \
		-I $(TOPDIR)/bt/ip/ble/lea/src \
		-I $(TOPDIR)/bt/ip/ble/lea/src/tmap \
		-I $(TOPDIR)/bt/ip/ble/lea/api \
		-I $(TOPDIR)/bt/ip/ble/iso/api \
		-I $(TOPDIR)/chip/${CHIP}/btos \
		-I $(TOPDIR)/chip/${CHIP}/btos/task/api \
		-I $(TOPDIR)/chip/${CHIP}/atcmd \
		-I $(TOPDIR)/chip/${CHIP}/cli_cmd \
		-I $(TOPDIR)/chip/${CHIP}/include/bt \
		-I $(TOPDIR)/chip/${CHIP}/bt_hal \
		-I $(TOPDIR)/chip/${CHIP}/include/ipc \
		-I $(TOPDIR)/chip/${CHIP}/ic_lock \
		-I $(TOPDIR)/chip/${CHIP}/event \
		-I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
		-I $(TOPDIR)/modules/rtos/include \
		-I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
		-I $(TOPDIR)/src/${APP_PATH}/if/aud_if \
		-I ./api		\
		-I ./task/api	\
		-I ./if/app_if	\
		-I ./if/aud_if	\
		-I ./if/bt_if	\
		-I ./pm		\
		-I ./src		\
		-I .

# copy cflags from RW IP
#CFLAGS += -DCFG_STATIC

CONFIG_HARTID=1
export CONFIG_HARTID
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


CFG_AMP_IPC_MASTER := 1
CFG_AMP_IPC_BT_CHAN := 1
CFG_AMP_IPC_WIFI_CHAN := 1
CFG_AMP_IPC_TCPIP := 1
CFG_AMP_IPC_MRPC_SERVER := 1
CFG_AMP_IPC_MRPC_CLIENT := 1
CFG_AMP_IPC_MRPC_SERVER_LWIP := 1
CFG_AMP_IPC_MRPC_CLIENT_BT :=1
CFG_AMP_IPC_MRPC_SERVER_BT_S2M :=1
CFG_AMP_IPC_MRPC_CLIENT_WIFI := 1
CFG_AMP_IPC_MRPC_SERVER_NVS  := 1
CFG_AMP_IPC_MRPC_SERVER_FLASH_IF := 1
CFG_AMP_IPC_FLASH_AGENT := 1
CFG_AMP_IPC_HALT_PEER_CORE := 1
CFG_AMP_IPC_HALT_BY_PEER_CORE := 1
CFG_AMP_IPC_INDICATION := 1
CONFIG_SKIP_BOOTCLOCK = 1

CFG_FLASH_IF := 1
export CFG_RTOS CONFIG_RTOS_AL CFG_IPERF CFG_PING CFG_BACK_TRACE CFG_AMP_IPC_MASTER CONFIG_SKIP_BOOTCLOCK CFG_AMP_IPC_MRPC_SERVER CFG_AMP_IPC_MRPC_CLIENT_BT CFG_AMP_IPC_MRPC_SERVER_BT_S2M
export CFG_AMP_IPC_MRPC_CLIENT CFG_AMP_IPC_MRPC_SERVER_LWIP CFG_AMP_IPC_MRPC_CLIENT_WIFI CFG_WIFI_MFG CFG_FLASH_IF
export CFG_AMP_IPC_MRPC_SERVER_NVS CFG_AMP_IPC_FLASH_AGENT CFG_AMP_IPC_BT_CHAN CFG_AMP_IPC_WIFI_CHAN CFG_AMP_IPC_HALT_PEER_CORE CFG_AMP_IPC_TCPIP CFG_AMP_IPC_MRPC_SERVER_FLASH_IF CFG_AMP_IPC_HALT_BY_PEER_CORE
export CFG_AMP_IPC_INDICATION

# mrpc server: GPADC get temp .. on IPC master side
CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M  := 1
export  CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M

CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S  := 0
export  CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S


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

CFG_NVS = 1
export CFG_NVS
CONFIG_NVS_FLASH_XIP = 1
export CONFIG_NVS_FLASH_XIP

ifeq ($(SYS_PSM), 1)
CFLAGS += -DSYS_PSM
endif

TX_BUF_COPY ?= 0
ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif
export TX_BUF_COPY






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



CONFIG_RAM_CONSTRAINED=1
ifeq ($(strip $(CONFIG_RAM_CONSTRAINED)), 1)
CFLAGS += -DCONFIG_RAM_CONSTRAINED
export CONFIG_RAM_CONSTRAINED
endif

CFG_MEMDUMP := 0
ifeq ($(strip $(CFG_MEMDUMP)), 1)
CFLAGS += -DCFG_MEMDUMP
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
CONFIG_RTOS_AL = 1
export CONFIG_RTOS_AL

CFG_SHORT_TCP_WND = 1
export CFG_SHORT_TCP_WND




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
	@sh "$(TOPDIR)/tools/mkhdr.sh" $(TGTOUT)/$(TARGET).bin
	#./merge_cp.sh $(TGTOUT)/cp_load.bin $(TGTOUT)/$(TARGET).bin 0x00010000
	python ./merge_cp.py $(TGTOUT)/ipc_wifi_bt_demo_ap.bin $(TGTOUT)/$(TARGET).bin 0x00180000 $(TGTOUT)/ipc_wifi_bt_demo.bin
	



include $(TOPDIR)/rules.mk
#LDSCRIPT  = ./flash.ld
ifeq ($(PSRAM_SEC),1)
LDSCRIPT   = $(TOPDIR)/chip/${CHIP}/arch/psram.ld
else
ifeq ($(CONFIG_HARTID),1)
LDSCRIPT   = ./flash_cp.ld
else
LDSCRIPT   = ./flash_ap.ld
endif
endif

