TOPDIR=../../../../

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= ap_host

export APP_PATH=arcs/ipc_demo/ap

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
         -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_core \
         -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_master \
         -I $(TOPDIR)/chip/${CHIP}/include/ipc \
         -I $(TOPDIR)/chip/${CHIP}/cli_cmd \
         -I $(TOPDIR)/chip/${CHIP}/event \
         -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include\
         -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure


# copy cflags from RW IP
CFLAGS += -DCFG_STATIC  -DCFG_ROM

#BUILD_ROM=1

#OPTMIZED_SIZE
BUILD_NANO=1
#Support PSRAM section
PSRAM_SEC=0
#SYS_TRACE=1
#export SYS_TRACE
# wifi memory optimization
CFG_MEM_OPT ?= on
export CFG_MEM_OPT

# Note: WCN_TYPE=wf:WiFi only, WCN_TYPE=bt:BT only, WCN_TYPE=both:Both support for Wifi and BT
WCN_TYPE=wf
export WCN_TYPE


ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
endif

CFG_RTOS := 1
CONFIG_RTOS_AL := 1
CFG_IPERF := 1
CFG_PING := 1
CFG_BACK_TRACE := 1

CFG_AMP_IPC_MASTER := 1
CFG_AMP_IPC_WIFI_CHAN := 1
CFG_AMP_IPC_TCPIP := 1
CFG_AMP_IPC_MRPC_SERVER := 1
CFG_AMP_IPC_MRPC_CLIENT := 1
CFG_AMP_IPC_MRPC_SERVER_LWIP := 1
CFG_AMP_IPC_MRPC_CLIENT_WIFI := 1
CFG_AMP_IPC_MRPC_SERVER_NVS  := 1
CFG_AMP_IPC_MRPC_SERVER_FLASH_IF := 1
CFG_AMP_IPC_FLASH_AGENT := 1
CFG_AMP_IPC_HALT_PEER_CORE := 1
CFG_AMP_IPC_HALT_BY_PEER_CORE := 1
CONFIG_SKIP_BOOTCLOCK = 1
CFG_WIFI_MFG := 1
CFG_FLASH_IF := 1
export CFG_RTOS CONFIG_RTOS_AL CFG_IPERF CFG_PING CFG_BACK_TRACE CFG_AMP_IPC_MASTER CONFIG_SKIP_BOOTCLOCK CFG_AMP_IPC_MRPC_SERVER
export CFG_AMP_IPC_MRPC_CLIENT CFG_AMP_IPC_MRPC_SERVER_LWIP CFG_AMP_IPC_MRPC_CLIENT_WIFI CFG_WIFI_MFG CFG_FLASH_IF
export CFG_AMP_IPC_MRPC_SERVER_NVS CFG_AMP_IPC_FLASH_AGENT CFG_AMP_IPC_WIFI_CHAN CFG_AMP_IPC_HALT_PEER_CORE CFG_AMP_IPC_TCPIP CFG_AMP_IPC_MRPC_SERVER_FLASH_IF CFG_AMP_IPC_HALT_BY_PEER_CORE

# mrpc server: GPADC get temp .. on IPC master side
CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M  := 1
export  CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M

CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S  := 1
export  CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S


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

ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
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



include $(TOPDIR)/rules.mk
LDSCRIPT   = ./flash.ld
