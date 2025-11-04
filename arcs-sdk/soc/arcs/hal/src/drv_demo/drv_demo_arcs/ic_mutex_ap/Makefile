TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = ic_mutex_test_ap
#TARGET = dma_xos_chk

# modules depend by this module
MODULES = bsp driver  rtos urpc_client lsf_client ic_lock

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
LIBS	= -lbsp -lrtos -lurpc_client -llsf_client -lic_lock

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=$(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I $(TOPDIR)/chip/${CHIP}/include/urpc \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_client \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_client/fifo \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_client/ic_mutex

# 注意: lib.a与app的Makefile定义一致, 因影响.h文件
CFLAGS += -DIC_USE_RTOS=1 -DIC_HAL_DCACHE_SIZE=32 -DIC_HAL_DCACHE_IS_COHERENT=1

# comiple with vanilla FreeRTOS:
# make ... CONFIG_RTOS_AL=0
#export the FreeRTOSConfig.h path 
#defined as CONFIG_FREERTOSCONFIG_H_PATH for rtos.a compiling
export CONFIG_FREERTOSCONFIG_H_PATH = src/drv_demo/drv_demo_arcs/ic_mutex_ap

ifneq ($(strip $(CONFIG_QEMU_N300)),)
CFLAGS += -DCONFIG_QEMU_N300
endif

CFG_RTOS=1
export CFG_RTOS
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

include $(TOPDIR)/rules.mk

LDSCRIPT   = ld/ram.ld
