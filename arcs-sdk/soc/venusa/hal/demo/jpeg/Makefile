TOPDIR=../../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= jpeg

# modules depend by this module
MODULES = bsp driver ls_jpeg

#libraries depend
LIBS	= -lbsp -lls_jpeg

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=$(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS  = $(shell find ./resource -name "*.c")
CSRCS += ./jpeg_check.c

#includings and flags
CFLAGS = -I $(TOPDIR)/include/CMSIS \
         -I $(TOPDIR)/include/CMSIS/core \
         -I $(TOPDIR)/include/NMSIS/Core/Include \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/modules/segger_rtt/include \
		 -I $(TOPDIR)/include \
		 -I $(TOPDIR)/chip/${CHIP}/bsp \
		 -I $(TOPDIR)/chip/${CHIP}/include/register \
		 -I $(TOPDIR)/chip/${CHIP}/include \
		 -I $(TOPDIR)/chip/${CHIP}/modules/ls_jpeg \


include $(TOPDIR)/rules.mk
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
LDSCRIPT  = ./ram.ld
