TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= cp_load

# modules depend by this module
MODULES = bsp driver debug

PSRAM_SEC ?=0

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	= -lbsp
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

#includings and flags
ifeq ($(PSRAM_SEC), 1)
CFLAGS += -DPSRAM_SEC=1
else
CFLAGS =
endif

ext:
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin
include $(TOPDIR)/rules.mk

ifeq ($(IC_BOARD), 1)
LDSCRIPT   = ./ilm_dlm.ld
endif
