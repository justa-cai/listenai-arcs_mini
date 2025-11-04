TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= ap_demo

CP_TARGET = cp_demo

export APP_PATH=arcs/ap_demo
export AP_TARGET=arcs/ap_demo

# modules depend by this module
MODULES = bsp driver debug rtos ipc ic_lock

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp -lrtos -lipc -lic_lock
ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga 
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	=
SSRCS	= $(wildcard *.S)

CFG_AMP_IPC := 1
CFG_AMP_IPC_MASTER := 1
CFG_AMP_IPC_MRPC_CLIENT := 1
CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF := 1
export CFG_AMP_IPC CFG_AMP_IPC_MASTER CFG_AMP_IPC_MRPC_CLIENT CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF

#includings and flags
CFLAGS = -I $(TOPDIR)/include/CMSIS \
         -I $(TOPDIR)/include/CMSIS/core \
         -I $(TOPDIR)/chip/${CHIP}/include/urpc \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_client \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_client/fifo \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_client/ic_mutex \
         -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
         -I $(TOPDIR)/chip/${CHIP}/include \
         -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/modules/nvs/include	\
		 -I $(TOPDIR)/modules/ota/include	\
		 -I $(TOPDIR)/modules/zbar/include	\
		 -I $(TOPDIR)/modules/rtos/include	\
         -I $(TOPDIR)/include/driver \
         -I $(TOPDIR)/include/debug \
         -I $(TOPDIR)/include/ \
         -I $(TOPDIR)/chip/${CHIP}/include/utils \
         -I $(TOPDIR)/chip/${CHIP}/include/ipc \
         -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_core \
         -I $(TOPDIR)/chip/$(CHIP)/ipc/ipc_utils \
         -I $(TOPDIR)/chip/${CHIP}/ipc/ipc_master \
         -I $(TOPDIR)/chip/${CHIP}/ic_lock \
         -I .

export BUILD_DATE := $(shell date "+0x%Y%m%d")
export CONFIG_SKIP_BOOTCLOCK=1

CFLAGS += -DBUILD_DATE=${BUILD_DATE} 

CFLAGS += -DIC_USE_RTOS=1 -DIC_HAL_DCACHE_SIZE=32 -DIC_HAL_DCACHE_IS_COHERENT=1

#export CONFIG_FREERTOSCONFIG_H_PATH = src/arcs/ap_demo

# entire flash encrypt enable
export FLASH_ENC_ALL=0
CFLAGS += -DFLASH_ENC_ALL=${FLASH_ENC_ALL}

CFG_RTOS=1
export CFG_RTOS
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

CONFIG_RTOS_AL = 1
export CONFIG_RTOS_AL

CONFIG_HARTID=0
export CONFIG_HARTID

CFG_AMP_IPC_HALT_BY_PEER_CORE = 1
export CFG_AMP_IPC_HALT_BY_PEER_CORE

include $(TOPDIR)/rules.mk

LDSCRIPT   = ./flash_ap.ld

pre:
	make -C ${TOPDIR}/src/${CHIP}/flash_boot apps CHIP=${CHIP} TGT=flash_boot IC_BOARD=${IC_BOARD} INNER=0 AP_TARGET=${AP_TARGET} FLASH_ENC_ALL=${FLASH_ENC_ALL} CFG_RTOS=0
	make -C ${TOPDIR}/src/${CHIP}/${CP_TARGET} apps CHIP=${CHIP} TGT=${CP_TARGET} IC_BOARD=${IC_BOARD} INNER=0 AP_TARGET=${AP_TARGET} FLASH_ENC_ALL=${FLASH_ENC_ALL} CONFIG_HARTID=1 CFG_AMP_IPC_MASTER=0 CFG_AMP_IPC_HALT_BY_PEER_CORE=0 CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF=0

ext: 
	sh ${TOPDIR}/tools/mkhdr_ota.sh $(TGTOUT)/$(TARGET).bin
	sh ${TOPDIR}/tools/merge.sh $(TGTOUT)/ap_demo_flash.bin $(TGTOUT)/flash_boot.bin $(TGTOUT)/$(TARGET).bin $(TGTOUT)/${CP_TARGET}.bin
