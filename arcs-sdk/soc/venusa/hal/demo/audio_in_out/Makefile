TOPDIR=../../../..

#ifeq ($(strip $(OS_DEF)),-DXOS_USED)
#$(info "Make nothing when XOS_USED is define")
#else

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= 
TARGET = audio_in_out

# modules depend by this module
MODULES = bsp driver 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
LIBS	= -lbsp 

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

#endif

include $(TOPDIR)/rules.mk

LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
