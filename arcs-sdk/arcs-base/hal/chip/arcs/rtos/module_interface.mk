
# vanilla FreeRTOS
CFLAGS += -I $(TOPDIR)/modules/rtos/include \
          -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure

# rtos_al
CFLAGS += -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al
