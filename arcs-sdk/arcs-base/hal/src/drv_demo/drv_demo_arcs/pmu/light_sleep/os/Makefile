TOPDIR=../../../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = os_light_sleep

# modules depend by this module
MODULES = bsp driver unity rtos

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
LIBS	= -lbsp -lunity -lrtos

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
SSRCS	= $(wildcard *.S)

#includings and flags
CFLAGS = -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/include/debug \
		 -I $(TOPDIR)/modules/unity/include \
		 -I $(TOPDIR)/modules/rtos/include \
		 -I $(TOPDIR)/include \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
		 -I $(TOPDIR)/chip/${CHIP}/include \
		 -I $(TOPDIR)/chip/${CHIP}/driver/private_include/segger




export CONFIG_FREERTOSCONFIG_H_PATH = src/drv_demo/drv_demo_arcs/pmu/light_sleep/os
export CFG_RTOS=1




include $(TOPDIR)/rules.mk

#LDSCRIPT = $(TOPDIR)/chip/${CHIP}/arch/ram.ld

LDSCRIPT = ./flash.ld
#LDSCRIPT = ./pmu_flash.ld



#ifeq ($(CONFIG_HARTID),0)
#LDSCRIPT = ./pmu_ap_run_flash.ld
#else
#LDSCRIPT = ./pmu_cp_run_flash.ld
#endif




ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
