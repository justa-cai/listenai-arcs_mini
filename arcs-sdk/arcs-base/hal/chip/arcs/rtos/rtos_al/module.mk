
# module使用者的 TOPDIR
vpath %.c $(TOPDIR)/chip/${CHIP}/rtos/rtos_al
vpath %.h $(TOPDIR)/chip/${CHIP}/rtos/rtos_al

# flags
module_cflags +=

# module使用者的 TOPDIR
module_includes += -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al

#files want to build: eg. CSRCS=a.c
module_sources_c += rtos_al.c free_rtos_hook.c

#files want to build: eg. SSRCS=a.S
module_sources_s +=
