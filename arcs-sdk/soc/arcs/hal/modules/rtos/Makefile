TOPDIR=../..

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
CSRCS	= event_groups.c list.c queue.c stream_buffer.c tasks.c timers.c \
          portable/MemMang/heap_4.c \
          portable/${BUILDTOOL}/non_secure/port.c

CPPSRCS	=
SSRCS	= 

ifeq ("${CHIP}", "arcs")
SSRCS += portable/${BUILDTOOL}/non_secure/portasm.S
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include
endif
ifeq ("${CHIP}", "mars")
SSRCS += portable/${BUILDTOOL}/non_secure/portasm.S
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include
endif
ifeq ("${CHIP}", "vegah")
SSRCS += portable/${BUILDTOOL}/non_secure/portasm.S
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include
endif
ifeq ("${CHIP}", "venus")
CSRCS += portable/${BUILDTOOL}/non_secure/portasm.c
CFLAGS  = -I $(TOPDIR)/include/CMSIS/core \
          -I $(TOPDIR)/include/CMSIS
endif
ifeq ("${CHIP}", "venusa")
SSRCS += portable/${BUILDTOOL}/non_secure/portasm.S
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include
endif

CFLAGS  += -I portable/${BUILDTOOL}/non_secure \
          -I $(TOPDIR)/modules/rtos/include \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/debug \
          -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
          -I $(TOPDIR)/chip/${CHIP}/bsp \
          -I $(TOPDIR)/chip/${CHIP}/include

#decide to include the application specific config or default one
ifneq ($(strip $(CONFIG_FREERTOSCONFIG_H_PATH)),)
$(info "building FreeRTOS library: configured as $(CONFIG_FREERTOSCONFIG_H_PATH)")
CFLAGS  += -I $(TOPDIR)/$(CONFIG_FREERTOSCONFIG_H_PATH)
else
CFLAGS  += -I $(TOPDIR)/chip/${CHIP}/rtos_default_config
endif


ifeq ($(SYS_TRACE), 1)
CFLAGS += -DSYS_TRACE
endif

include $(TOPDIR)/rules.mk
