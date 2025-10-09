
# vanilla FreeRTOS
CFLAGS += -I $(TOPDIR)/modules/rtos/include \
          -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure

#decide to include the application specific config or default one
ifneq ($(strip $(CONFIG_FREERTOSCONFIG_H_PATH)),)
$(info "FreeRTOS interface :: FreeRTOSConfig.h :: $(CONFIG_FREERTOSCONFIG_H_PATH)")
CFLAGS  += -I $(TOPDIR)/$(CONFIG_FREERTOSCONFIG_H_PATH)
AFLAGS  += -I $(TOPDIR)/$(CONFIG_FREERTOSCONFIG_H_PATH)
else
$(info "FreeRTOS interface :: FreeRTOSConfig.h :: $(TOPDIR)/chip/${CHIP}/rtos_default_config")
CFLAGS  += -I $(TOPDIR)/chip/${CHIP}/rtos_default_config
AFLAGS  += -I $(TOPDIR)/$(CONFIG_FREERTOSCONFIG_H_PATH)
endif