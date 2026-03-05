
MODULES =
LIBS    =

ifeq ($(MODULE_BSP), 1)
MODULES += bsp
LIBS    += -lbsp
endif

ifeq ($(MODULE_DRV), 1)
MODULES += driver
ifeq ($(LIB_DRV_FPGA), 1)
LIBS	+= -ldrv_fpga
endif
ifeq ($(LIB_DRV), 1)
LIBS	+= -ldrv
endif
endif

ifeq ($(MODULE_DBG), 1)
MODULES += debug
LIBS    += -ldbg
endif

ifeq ($(MODULE_PATCH), 1)
MODULES += patch
LIBS    += -lpatch
endif

ifeq ($(MODULE_NVS), 1)
MODULES += nvs
LIBS    += -lnvs
endif

ifeq ($(MODULE_URPC_SER), 1)
MODULES += urpc_server
LIBS    += -lurpc_server
endif

ifeq ($(MODULE_LSF_SER), 1)
MODULES += lsf_server
LIBS    += -llsf_server
endif

ifeq ($(MODULE_URPC_CLI), 1)
MODULES += urpc_client
LIBS    += -lurpc_client
endif

ifeq ($(MODULE_LSF_CLI), 1)
MODULES += lsf_client
LIBS    += -llsf_client
endif

ifeq ($(MODULE_SHELL), 1)
MODULES += shell
LIBS    += -lshell
endif

ifeq ($(MODULE_RTOS), 1)
MODULES += rtos
LIBS    += -lrtos
endif

ifeq ($(MODULE_WCND), 1)
MODULES += wcnd
LIBS    += -lwcnd
endif

ifeq ($(MODULE_VRTC), 1)
MODULES += vrtc
LIBS    += -lvrtc
endif

ifeq ($(MODULE_EVENT), 1)
MODULES += event
LIBS    += -levent
endif

ifeq ($(MODULE_ATCMD), 1)
MODULES += atcmd
LIBS    += -latcmd
export CFG_ATCMD=1
endif

ifeq ($(MODULE_CLICMD), 1)
MODULES += cli_cmd
LIBS    += -lclicmd
endif

ifeq ($(MODULE_UTILS), 1)
MODULES += utils
LIBS    += -lutils
endif

ifeq ($(MODULE_MEMDUMP), 1)
MODULES += memdump
LIBS    += -lmemdump
endif

ifeq ($(MODULE_IPC), 1)
MODULES += ipc
LIBS	+= -lipc
endif

ifeq ($(MODULE_WLIF), 1)
MODULES += wlif
LIBS	+= -lwlif
endif

ifeq ($(MODULE_LWIP), 1)
MODULES += lwip
LIBS	+= -llwip
endif

ifeq ($(MODULE_MBDTLS), 1)
MODULES += mbedtls
LIBS    += -lmbedtls
endif

export WIFI_OTA := 0
ifeq ($(MODULE_WOTA), 1)
MODULES += wota
LIBS   += -lwota
export WIFI_OTA := 1
endif

ifeq ($(MODULE_WIFI), 1)
LIBS	+= -llmac -llmacdp -lumac -lumacdp -lplf -lmodule -lmoduledp
MODULES += wifi

ifeq ($(LIB_WPASTA), 1)
LIBS	+= -lwpa_supplicant_sta
endif
ifeq ($(LIB_WPAAP), 1)
LIBS	+= -lwpa_supplicant_ap
endif
endif

ifneq ($(MODULE_LWIP), 1)
ifeq ($(CFG_AMP_IPC), 1)
LIBS    += -lsimsocket
MODULES += simsocket
endif
endif

ifeq ($(MODULE_BT), 1)
ifeq ($(INNER), 1)
LIBS += -lble -lbt_base #-lbt
MODULES  += bt
else
#EXTLIBS += -lble -lbt_base -L./lib
endif
endif

ifeq ($(MODULE_BTOS), 1)
LIBS += -lbtos
MODULES  += btos
endif

ifeq ($(MODULE_LUNA), 1)
LIBS += -lluna
MODULES  += luna
endif

ifeq ($(MODULE_CODEC), 1)
LIBS += -lcodec
MODULES  += codec
endif

ifeq ($(MODULE_AUDIO), 1)
LIBS += -laudio
MODULES  += audio
endif

ifeq ($(MODULE_FLASH_IF), 1)
LIBS += -lflash_if
MODULES  += flash_if
endif

ifeq ($(MODULE_IC_LOCK), 1)
LIBS += -lic_lock
MODULES  += ic_lock
endif

ifeq ($(MODULE_PM), 1)
LIBS += -lpm
MODULES  += pm_impl
endif

ifeq ($(LIB_EXT_WIFI_BT), 1)
EXT_LIBS  := $(shell ls $(TOPDIR)/chip/${CHIP}/lib | sed 's/^lib/-l/; s/\.[^.]*$$//')
EXTLIBS += $(EXT_LIBS) -L$(TOPDIR)/chip/${CHIP}/lib
endif

#includings and flags
CFLAGS = -I $(TOPDIR)/include/CMSIS \
         -I $(TOPDIR)/include/CMSIS/core \
         -I $(TOPDIR)/chip/${CHIP}/include \
         -I $(TOPDIR)/chip/${CHIP}/include/net \
         -I $(TOPDIR)/chip/${CHIP}/include/wifi \
         -I $(TOPDIR)/chip/${CHIP}/include/utils \
         -I $(TOPDIR)/chip/${CHIP}/wcnd/include/bt_inc \
         -I $(TOPDIR)/wifi/macsw/ip/lmac/src/rwnx \
         -I $(TOPDIR)/wifi/macsw/modules/common/src \
         -I $(TOPDIR)/wifi/macsw/modules/rtos/src \
         -I $(TOPDIR)/wifi/macsw/plf/refip/src/driver \
         -I $(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
         -I $(TOPDIR)/chip/${CHIP}/atcmd \
         -I $(TOPDIR)/chip/${CHIP}/cli_cmd \
	 -I $(TOPDIR)/chip/$(CHIP)/lwip/port/include \
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
         -I $(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include\
         -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
         -I $(TOPDIR)/modules/shell \
         -I .

SDK_HEADER_FILE = $(TOPDIR)/chip/${CHIP}/include/sdk_version.h
