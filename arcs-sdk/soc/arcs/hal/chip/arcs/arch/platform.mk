# include by rules.mk
# define platfrom specification


CROSS_COMPILE := $(subst \,/,$(NUCLEI_TOOLCHAIN_PATH))/bin/riscv64-unknown-elf-

ifeq ($(HARD_FLOAT),1)
ifeq ($(CONFIG_HARTID),1)
COREFLAGS := -march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32 -mcmodel=medlow -mtune=nuclei-300-series --specs=nosys.specs
else
COREFLAGS := -march=rv32imafc_zba_zbb_zbc_zbs -mabi=ilp32f -mcmodel=medlow -mtune=nuclei-300-series --specs=nosys.specs
endif
else
COREFLAGS := -march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32 -mcmodel=medlow -mtune=nuclei-300-series --specs=nosys.specs
endif

AFLAGS    += -x assembler-with-cpp

CFLAGS    += -I $(TOPDIR)/chip/$(CHIP)/bsp \
			 -I $(TOPDIR)/chip/$(CHIP)/include \
			 -I $(TOPDIR)/chip/$(CHIP)/include/register \
			 -I $(TOPDIR)/modules/nvs/include \
			 -I $(TOPDIR)/include/NMSIS/Core/Include \
             -I $(TOPDIR)/include/bsp

# rtos module customization
ifeq ($(CFG_RTOS),1)

CFLAGS    += -DCFG_RTOS

ifeq ($(CFG_RTOS_SMP),1)
AFLAGS    += -DSMP_CPU_CNT=2
CFLAGS    += -DSMP_CPU_CNT=2
LDFLAGS   += -Wl,-defsym=__SMP_CPU_CNT=2

AFLAGS    += -DconfigNUMBER_OF_CORES=2
# CFLAGS    += -DconfigNUMBER_OF_CORES=2
endif

ifeq ($(CONFIG_RTOS_AL),1)

$(info "using rtos_al")

# rtos module customization -TARGET.
# Difference: TARGET has MODULES, LIB does not.
ifneq ($(strip $(MODULES)),)
# check if rtos module is included
ifneq ($(findstring rtos,$(MODULES)),)

ifneq ($(findstring -lrtos,$(LIBS)),-lrtos)
$(error "rtos module is included, but -lrtos is not found in LIBS")
endif

# filter out the rtos module
MODULES := $(filter-out rtos, $(MODULES))

# define rtos target
.PHONY: rtos_custom
mk_libs : rtos_custom

rtos_custom :
	if [ -f "$(TOPDIR)/chip/$(CHIP)/rtos/Makefile" ]; then $(MAKE) -C $(TOPDIR)/chip/$(CHIP)/rtos $(MKDEFS) libs ||exit 1; fi;

endif # ifneq ($(findstring rtos,$(MODULES)),)
endif # ifneq ($(strip $(MODULES)),)

# rtos module customization - TARGET & modules
# rtos include path & FreeRTOSConfig.h path
include $(TOPDIR)/chip/$(CHIP)/rtos/module_interface.mk

else # ifeq ($(CONFIG_RTOS_AL),1)


# rtos module customization -TARGET.
# Difference: TARGET has MODULES, LIB does not.
ifneq ($(strip $(MODULES)),)
# check if rtos module is included
ifneq ($(findstring rtos,$(MODULES)),)

ifneq ($(findstring -lrtos,$(LIBS)),-lrtos)
$(error "rtos module is included, but -lrtos is not found in LIBS")
endif

# filter out the rtos module
MODULES := $(filter-out rtos, $(MODULES))

# define rtos target
.PHONY: rtos_custom
mk_libs : rtos_custom

rtos_custom :
	if [ -f "$(TOPDIR)/modules/rtos/Makefile" ]; then $(MAKE) -C $(TOPDIR)/modules/rtos $(MKDEFS) libs ||exit 1; fi;

endif # ifneq ($(findstring rtos,$(MODULES)),)
endif # ifneq ($(strip $(MODULES)),)

# rtos module customization - TARGET & modules
# rtos include path & FreeRTOSConfig.h path
include $(TOPDIR)/modules/rtos/module_interface.mk

endif # ifeq ($(CONFIG_RTOS_AL),1)

endif # ifeq ($(CFG_RTOS),1)

ifeq ($(CONFIG_TRACE), rtt)
COREFLAGS += -DCONFIG_TRACE

CFLAGS    += -I $(TOPDIR)/chip/$(CHIP)/TraceRecorder/include \
			 -I $(TOPDIR)/chip/$(CHIP)/TraceRecorder/config \
			 -I $(TOPDIR)/chip/$(CHIP)/TraceRecorder/streamports/Jlink_RTT/include \
			 -I $(TOPDIR)/chip/$(CHIP)/TraceRecorder/streamports/Jlink_RTT/config

# Nessessary only when compiling TARGET. Difference: TARGET has MODULES, LIB does not.
ifneq ($(strip $(MODULES)),)
MODULES   += TraceRecorder
LIBS    += -ltrace
endif

endif

CFLAGS    += \
	-Wall -ffunction-sections -fdata-sections -static -ffast-math -fno-common -fno-builtin-printf

ifeq ($(HARD_FLOAT),1)
ifeq ($(CONFIG_HARTID),1)
LDFLAGS   += \
	-march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32 -mtune=nuclei-300-series -static -Wl,-gc-sections -nostartfiles -Wl,--start-group -Wl,--end-group #--specs=nosys.specs -lstdc++
else
LDFLAGS   += \
	-march=rv32imafc_zba_zbb_zbc_zbs -mabi=ilp32f -mtune=nuclei-300-series -static -Wl,-gc-sections -nostartfiles -Wl,--start-group -Wl,--end-group #--specs=nosys.specs -lstdc++
endif
else
LDFLAGS   += \
	-march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32 -mtune=nuclei-300-series -static -Wl,-gc-sections -nostartfiles -Wl,--start-group -Wl,--end-group #--specs=nosys.specs -lstdc++
endif
#remove --specs=nosys.specs for warning when link libc.a
	
ifeq ($(BUILD_NANO),1)
EXTLIBS   += -lc_nano -lg_nano 
else
EXTLIBS   += -lc -lgcc #-lc_nano -lg_nano #-lstdc++_nano -lsupc++_nano #-lm 
endif


ifneq ($(strip $(CONFIG_QEMU_N300)),)
CFLAGS += -DCONFIG_QEMU_N300
endif


ifneq ($(strip $(CONFIG_QEMU_N300)),)
LDSCRIPT   ?= $(TOPDIR)/chip/$(CHIP)/arch/rom_qemu.ld
else
LDSCRIPT   ?= $(TOPDIR)/chip/$(CHIP)/arch/ram.ld
ifeq ($(CONFIG_HARTID),1)

COREFLAGS  += -DBOOT_HARTID=1
COREFLAGS  += -DMBX_CP_WORK=1
LDFLAGS    += -DBOOT_HARTID=1
else

COREFLAGS  += -DBOOT_HARTID=0
COREFLAGS  += -DMBX_AP_WORK=1
LDFLAGS    += -DBOOT_HARTID=0
endif
endif

ifneq ($(strip $(CONFIG_EXT_RAM)),)
CFLAGS += -DCONFIG_EXT_RAM
endif

ifneq ($(strip $(CONFIG_RAM_CONSTRAINED)),)
CFLAGS += -DCONFIG_RAM_CONSTRAINED
endif

ifneq ($(strip $(CFG_PING)),)
CFLAGS += -DCFG_PING
endif
ifneq ($(strip $(CFG_IPERF)),)
CFLAGS += -DCFG_IPERF
endif

ifneq ($(strip $(CFG_WIFI_MFG)),)
CFLAGS += -DCFG_WIFI_MFG
endif

ifneq ($(strip $(CFG_MEMDUMP)),)
CFLAGS += -DCFG_MEMDUMP
endif

ifneq ($(strip $(CFG_NVS)),)
CFLAGS += -DCFG_NVS=$(CFG_NVS)
endif

ifeq ($(strip $(CFG_MEM_OPT)), on)
CFLAGS += -DCFG_MEM_OPT=1
else
CFLAGS += -DCFG_MEM_OPT=0
endif

ifneq ($(strip $(CFG_EVENT_TASK_STACK_SIZE)),)
CFLAGS += -DCFG_EVENT_TASK_STACK_SIZE=$(CFG_EVENT_TASK_STACK_SIZE)
endif

ifneq ($(strip $(CFG_EVENT_TASK_PRIORITY)),)
CFLAGS += -DCFG_EVENT_TASK_PRIORITY=$(CFG_EVENT_TASK_PRIORITY)
endif

ifneq ($(strip $(CFG_TOTAL_HEAP_SIZE)),)
CFLAGS += -DCFG_TOTAL_HEAP_SIZE=$(CFG_TOTAL_HEAP_SIZE)
endif

ifneq ("$(WIFI_HOST_STANDALONE)", "")
CFLAGS    += -DWIFI_HOST_STANDALONE
endif

ifeq ($(CFG_ATCMD), 1)
CFLAGS += -DCFG_ATCMD
endif
ifeq ($(BT_WIFI_COEX), 1)
CFLAGS += -DBT_WIFI_COEX
endif
ifeq ("$(CFG_AMP_IPC)", "1")
CFLAGS += -DCFG_AMP_IPC=1
ifeq ("$(CFG_AMP_IPC_MASTER)", "1")
CFLAGS += -DCFG_AMP_IPC_MASTER=1
else
CFLAGS += -DCFG_AMP_IPC_SLAVE=1
endif
ifeq ("$(CFG_IPC_PRINT)", "1")
CFLAGS += -DCFG_IPC_PRINT=1
endif
ifeq ("$(CFG_AMP_IPC_WIFI_CHAN)", "1")
CFLAGS += -DCFG_AMP_IPC_WIFI_CHAN=1
endif
ifeq ("$(CFG_AMP_IPC_FLASH_AGENT)", "1")
CFLAGS += -DCFG_AMP_IPC_FLASH_AGENT=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_SERVER)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_SERVER=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_CLIENT=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_LWIP)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_SERVER_LWIP=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_LWIP)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_CLIENT_LWIP=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_WIFI)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_SERVER_WIFI=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_WIFI)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_CLIENT_WIFI=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_NVS)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_CLIENT_NVS=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_NVS)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_SERVER_NVS=1
endif
ifeq ("$(CFG_AMP_IPC_HALT_PEER_CORE)", "1")
CFLAGS += -DCFG_AMP_IPC_HALT_PEER_CORE=1
endif
ifeq ("$(CFG_AMP_IPC_HALT_BY_PEER_CORE)", "1")
CFLAGS += -DCFG_AMP_IPC_HALT_BY_PEER_CORE=1
endif
ifeq ("$(CFG_AMP_IPC_TCPIP)", "1")
CFLAGS += -DCFG_AMP_IPC_TCPIP=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_FLASH_IF)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_SERVER_FLASH_IF=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_CLIENT_FLASH_IF=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_UTILS)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_CLIENT_UTILS=1
endif
ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_UTILS)", "1")
CFLAGS += -DCFG_AMP_IPC_MRPC_SERVER_UTILS=1
endif
endif

ifeq ("$(CFG_FLASH_IF)", "1")
CFLAGS += -DCFG_FLASH_IF=1
endif

ifneq ($(strip $(CONFIG_USE_RTT)),)
CFLAGS  += -DCONFIG_USE_RTT
CFLAGS  += -I $(TOPDIR)/modules/segger_rtt/include
MODULES += segger_rtt
LIBS    += -lsegger_rtt
endif

ifeq ("$(strip $(CONFIG_SKIP_BOOTCLOCK))","1")
CFLAGS  += -DCONFIG_SKIP_BOOTCLOCK
endif

ifeq ("$(strip $(CALI_BUF))", "1")
CFLAGS += -DCALI_BUF=1
else
CFLAGS += -DCALI_BUF=0
endif

ifneq ($(strip $(CONFIG_DUAL_FLASH)),)
CFLAGS += -DCONFIG_DUAL_FLASH
endif