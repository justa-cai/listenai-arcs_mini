TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= liblsf_server.a
TARGET	=

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs= $(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= $(wildcard *.c) $(wildcard mtrace/*.c) $(wildcard ic_mutex/*.c)
CPPSRCS	=
SSRCS	=

#includings and flags
CFLAGS = -I./ic_mutex \
		 -I./mtrace \
		 -I. \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/include/debug \
		 -I $(TOPDIR)/chip/${CHIP}/include/urpc \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_server \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_server/fifo \
		 -I $(TOPDIR)/chip/${CHIP}/include/lsf_server/ic_mutex

# 注意: lib.a与app的Makefile定义一致, 因影响.h文件
CFLAGS += -DPLATFORM_CP -DIC_USE_RTOS=1 -DIC_HAL_DCACHE_SIZE=32 -DIC_HAL_DCACHE_IS_COHERENT=1

ifneq ($(strip $(CONFIG_QEMU_N300)),)
CFLAGS += -DCONFIG_QEMU_N300
endif

CFG_RTOS=1
export CFG_RTOS
ifeq ($(CFG_RTOS), 1)
CFLAGS += -DCFG_RTOS
endif

include $(TOPDIR)/rules.mk
