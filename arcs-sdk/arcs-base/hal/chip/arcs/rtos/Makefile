TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= librtos.a
TARGET	= 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=
ifeq ($(IC_BOARD), 0)
LIBS	+= 
else
LIBS	+= 
endif

ifneq ($(MAKECMDGOALS), clean)
ifeq ("${BUILDTOOL}", "")
$(info "Please set BUILDTOOL!!!")
exit
endif
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	=
CPPSRCS	=
SSRCS	= 
CFLAGS  =

CFLAGS  += -I $(TOPDIR)/include/bsp \
           -I $(TOPDIR)/include/debug \
           -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
           -I $(TOPDIR)/chip/${CHIP}/bsp \
           -I $(TOPDIR)/chip/${CHIP}/include

# submodules
include utility.mk

# 相对路径: 原生rtos
include $(TOPDIR)/modules/rtos/module.mk

# 相对路径: rtos_al
include rtos_al/module.mk

CSRCS	+= $(module_sources_c)
SSRCS	+= $(module_sources_s)
CFLAGS	+= $(module_cflags)
CFLAGS	+= $(module_includes)

# error if CONFIG_FREERTOSCONFIG_H_PATH defined
ifneq ($(strip $(CONFIG_FREERTOSCONFIG_H_PATH)),)
$(error "rtos_al with custom FreeRTOSConfig.h: $(CONFIG_FREERTOSCONFIG_H_PATH)")
endif

ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
endif

include $(TOPDIR)/rules.mk

