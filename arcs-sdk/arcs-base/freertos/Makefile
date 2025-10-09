TOPDIR  =../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= librtos.a
TARGET	= 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
EXDIRS  =

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	= event_groups.c list.c queue.c stream_buffer.c tasks.c timers.c \
          portable/MemMang/heap_4.c portable/port.c rtos_al.c

CPPSRCS	=
SSRCS	= 

SSRCS += portable/portasm.S
CFLAGS  = -I $(TOPDIR)/include/NMSIS/Core/Include

CFLAGS  += -I portable -I include \
          -I $(TOPDIR)/include/rtos \
          -I $(TOPDIR)/include/bsp \
          -I $(TOPDIR)/include/debug \
          -I $(TOPDIR)/chip/${CHIP}/driver/private_include \
          -I $(TOPDIR)/chip/${CHIP}/bsp \
          -I $(TOPDIR)/chip/${CHIP}/include \
          -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al

pre: 
	mkdir -p $(TOPDIR)/lib/${CHIP}-${TGT}/librtos
	@$(CC) -c $(CFLAGS) -std=gnu11 $(TOPDIR)/chip/${CHIP}/rtos/rtos_al/rtos_al.c -o $(TOPDIR)/lib/${CHIP}-${TGT}/librtos/rtos_al.o

include $(TOPDIR)/rules.mk

