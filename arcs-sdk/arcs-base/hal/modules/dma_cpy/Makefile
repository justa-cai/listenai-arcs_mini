TOPDIR=../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libopi_cpy.a
TARGET	= 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=
ifeq ($(IC_BOARD), 0)
LIBS	+= 
else
LIBS	+= 
endif

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
#CSRCS	= $(shell find . -name "*.c")
CSRCS	= dma_cpy_${CHIP}.c

#ifeq ("${CHIP}", "arcs")
#CSRCS	+=
#else ifeq ("${CHIP}", "mars")
#CSRCS	+=
#else
#$(info "No extra source files for" $(CHIP))
#endif

CPPSRCS	=
SSRCS	= 

#-mcoproc
CFLAGS =  -I $(TOPDIR)/chip/${CHIP}/bsp \
          -I $(TOPDIR)/chip/${CHIP}/include \
          -I $(TOPDIR)/include/NMSIS/Core/Include \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/rtos
 
include $(TOPDIR)/rules.mk

