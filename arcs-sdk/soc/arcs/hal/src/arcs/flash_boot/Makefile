TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= flash_boot

# modules depend by this module
MODULES = bsp driver nvs std

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp -lnvs -lstd
ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga 
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c)
CSRCS	+= 	$(wildcard src/*.c)

CPPSRCS	=
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS = -I $(TOPDIR)/include/CMSIS \
         -I $(TOPDIR)/include/CMSIS/core \
         -I $(TOPDIR)/src/${APP_PATH} \
         -I $(TOPDIR)/src/${AP_TARGET} \
         -I $(TOPDIR)/include/patch \
         -I $(TOPDIR)/include/nvs \
         -I $(TOPDIR)/include/bsp \
         -I $(TOPDIR)/include/driver \
         -I $(TOPDIR)/include/debug \
         -I $(TOPDIR)/modules/nvs/include	\
         -I $(TOPDIR)/chip/$(CHIP)/rom \
         -I $(TOPDIR)/chip/${CHIP}/include \
         -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
         -I ./api		\
         -I .

CONFIG_EXT_RAM=1
ifeq ($(strip $(CONFIG_EXT_RAM)), 1)
CFLAGS += -DCONFIG_EXT_RAM
export CONFIG_EXT_RAM
endif

export BUILD_DATE := $(shell date "+0x%Y%m%d")
export BUILD_NANO := 1
export CONFIG_SKIP_BOOTCLOCK=0

CFLAGS += -DBUILD_DATE=${BUILD_DATE}

CFLAGS += -DFLASH_MUTEX_LOCK=0

# define the macro 'FLASH_ENC_ALL' in ap's Makefile
CFLAGS += -DFLASH_ENC_ALL=${FLASH_ENC_ALL}

include $(TOPDIR)/rules.mk

LDSCRIPT   = $(TOPDIR)/chip/$(CHIP)/arch/flash_boot.ld

# compute checksum of header
ext: 
	@sh ${TOPDIR}/tools/mkhdr_ota.sh $(TGTOUT)/$(TARGET).bin

