
# module使用者的 TOPDIR
vpath %.c $(TOPDIR)/modules/rtos
vpath %.S $(TOPDIR)/modules/rtos
vpath %.h $(TOPDIR)/modules/rtos/include

# flags
module_cflags +=

# module使用者的 TOPDIR
module_includes += -I $(TOPDIR)/modules/rtos/include \
                   -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
                   -I $(TOPDIR)/include/NMSIS/Core/Include

#files want to build: eg. CSRCS=a.c
module_sources_c += event_groups.c \
                    list.c \
                    queue.c \
                    stream_buffer.c \
                    tasks.c \
                    timers.c \
                    portable/MemMang/heap_4.c \
                    portable/${BUILDTOOL}/non_secure/port.c

#files want to build: eg. SSRCS=a.S
module_sources_s += portable/${BUILDTOOL}/non_secure/portasm.S
