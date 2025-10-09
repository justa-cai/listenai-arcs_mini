TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
ifeq ($(IC_BOARD), 0)
LIB		= libdrv_fpga.a
else
LIB		= libdrv.a
endif

TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= 
ifeq ($(IC_BOARD), 0)
LIBS	+= 
else
LIBS	+= 
endif

#exclude subdirs
exclude_dirs= $(patsubst ./%,%,$(shell find . -maxdepth 3 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(shell find . -name "*.c")

ifneq (${CFG_RTOS},1)
# exclude files from ./amp_ipc/
CSRCS_TEMP := $(filter-out ./amp_ipc/%,$(CSRCS))
CSRCS	= $(CSRCS_TEMP)
endif # ifneq (${CFG_RTOS},1)

CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS =  -I $(TOPDIR)/include/NMSIS/Core/Include \
          -I $(TOPDIR)/chip/${CHIP}/bsp \
          -I $(TOPDIR)/chip/${CHIP}/include \
          -I $(TOPDIR)/chip/${CHIP}/include/register \
          -I $(TOPDIR)/chip/${CHIP}/driver/usb_subsys \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/bt/plf/api \
          -I $(TOPDIR)/modules/ota/include
          
CFLAGS += -DIC_USE_RTOS=1 -DIC_HAL_DCACHE_SIZE=32 -DIC_HAL_DCACHE_IS_COHERENT=1

#decide to include the application specific tusb_config.h or default one
ifneq ($(strip $(CFG_TUSB_CONFIG_H_PATHNAME)),)
$(info "PRIVATE tusb_config.h @ $(CFG_TUSB_CONFIG_H_PATHNAME)")
#absolute path
CFLAGS  += -DCFG_TUSB_CONFIG_FILE=\"$(CFG_TUSB_CONFIG_H_PATHNAME)\"
else
$(info "PUBLIC tusb_config.h @ $(TOPDIR)/chip/${CHIP}/driver/usb_subsys/tusb_config.h")
endif

ifeq ($(FLASH_ENC_ALL), )
FLASH_ENC_ALL = 0
endif

ifeq ($(BOOT_HARTID), 0)
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/include/lsf_client \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_client/fifo \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_client/ic_mutex
else
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/include/lsf_server \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_server/fifo \
		-I $(TOPDIR)/chip/${CHIP}/include/lsf_server/ic_mutex
endif

CFLAGS += -DFLASH_ENC_ALL=${FLASH_ENC_ALL}

include $(TOPDIR)/rules.mk

