TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB     = libbt_hal.a
TARGET  =

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS    =
ifeq ($(IC_BOARD), 0)
LIBS    +=
else
LIBS    +=
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
ifeq ("$(CFG_AMP_IPC_SLAVE)", "1")
CSRCS	= bt_stack_hal.c bt_ip_hal.c bt_app_hal.c
else
CSRCS	= $(shell find . -name "*.c")
endif
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include \
		-I $(TOPDIR)/include/debug \
		-I $(TOPDIR)/include/bsp \
		-I $(TOPDIR)/include/driver \
		-I $(TOPDIR)/include/rtos \
		-I $(TOPDIR)/chip/${CHIP}/bsp \
		-I $(TOPDIR)/chip/${CHIP}/include \
		-I $(TOPDIR)/chip/${CHIP}/include/wifi \
		-I $(TOPDIR)/chip/${CHIP}/include/bt \
		-I $(TOPDIR)/chip/${CHIP}/include/ipc \
		-I $(TOPDIR)/chip/${CHIP}/include/utils \
		-I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		-I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
		-I $(TOPDIR)/chip/${CHIP}/btos \
		-I $(TOPDIR)/chip/${CHIP}/btos/task/api \
		-I $(TOPDIR)/chip/${CHIP}/bt_hal \
		-I $(TOPDIR)/chip/${CHIP}/atcmd \
		-I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc \
		-I $(TOPDIR)/modules/rtos/include \
		-I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
		-I $(TOPDIR)/modules/profiles/ota/otas/api  \
		-I $(TOPDIR)/modules/profiles/hogp/hogpd/api \
		-I $(TOPDIR)/modules/profiles/bas/bass/api  \
		-I $(TOPDIR)/modules/profiles/dis/diss/api \
		-I $(TOPDIR)/modules/profiles/netcfg_ble/netcfg_bles/api  \
		-I $(TOPDIR)/modules/profiles/hogp \
		-I $(TOPDIR)/modules/codec/api \
		-I $(TOPDIR)/modules/audio/api \
		-I $(TOPDIR)/include/nvs \
		-I $(TOPDIR)/src/${APP_PATH} \
		-I $(TOPDIR)/src/${APP_PATH}/if/bt_if \
		  

include $(TOPDIR)/rules.mk

