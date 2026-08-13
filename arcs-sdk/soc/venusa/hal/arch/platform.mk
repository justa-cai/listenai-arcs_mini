# include by rules.mk
# define platfrom specification

CROSS_COMPILE := $(subst \,/,$(NUCLEI_TOOLCHAIN_PATH))/bin/riscv64-unknown-elf-

COREFLAGS := -march=rv32imafc_zba_zbb_zbc_zbs_xxldsp -mabi=ilp32f -mcmodel=medlow -mtune=nuclei-300-series --specs=nosys.specs

AFLAGS    += -x assembler-with-cpp

CFLAGS    += -I $(TOPDIR)/chip/${CHIP}/bsp \
             -I $(TOPDIR)/chip/${CHIP}/include \
             -I $(TOPDIR)/chip/${CHIP}/include/NMSIS/Core/Include \
             -I $(TOPDIR)/chip/${CHIP}/include/NMSIS/DSP/Include \
             -I $(TOPDIR)/chip/${CHIP}/include/register \
             -I $(TOPDIR)/include/bsp

ifeq ($(CONFIG_HARTID),1)
COREFLAGS  += -DBOOT_HARTID=1
LDFLAGS    += -DBOOT_HARTID=1
else
COREFLAGS  += -DBOOT_HARTID=0
LDFLAGS    += -DBOOT_HARTID=0
endif


ifneq ($(strip $(CONFIG_USE_SMP)),)
AFLAGS    += -DSMP_CPU_CNT=2
CFLAGS    += -DSMP_CPU_CNT=2
LDFLAGS   += -Wl,-defsym=__SMP_CPU_CNT=2
AFLAGS    += -DconfigNUMBER_OF_CORES=2
endif


# rtos module customization
ifeq (${CFG_RTOS},1)

#decide to include the application specific config or default one
ifneq ($(strip $(CONFIG_FREERTOSCONFIG_H_PATH)),)
$(info "building FreeRTOS library: FreeRTOSConfig.h in $(CONFIG_FREERTOSCONFIG_H_PATH)")
CFLAGS  += -I $(TOPDIR)/$(CONFIG_FREERTOSCONFIG_H_PATH)
else
CFLAGS  += -I $(TOPDIR)/chip/${CHIP}/rtos_default_config
endif

ifneq ($(wildcard $(TOPDIR)/chip/$(CHIP)/modules/rtos/include),)
$(info "                           include from $(TOPDIR)/chip/$(CHIP)/modules/rtos")
CFLAGS  += -I $(TOPDIR)/chip/$(CHIP)/modules/rtos/include
CFLAGS  += -I $(TOPDIR)/chip/$(CHIP)/modules/rtos/portable/${BUILDTOOL}/non_secure
else
$(info "                           include from $(TOPDIR)/modules/rtos")
CFLAGS  += -I $(TOPDIR)/modules/rtos/include
CFLAGS  += -I $(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure
endif

endif #CFG_RTOS


CFLAGS    += \
	-Wall -Werror -ffunction-sections -fdata-sections -static -ffast-math -fno-common -fno-builtin-printf -ffunction-sections -fdata-sections

LDFLAGS   += \
	-march=rv32imafc_zba_zbb_zbc_zbs_xxldsp -mabi=ilp32f -mtune=nuclei-300-series -static -Wl,-gc-sections -nostartfiles -Wl,--start-group -Wl,--end-group #--specs=nosys.specs -lstdc++ 
#remove --specs=nosys.specs for warning when link libc.a
	
ifeq ($(BUILD_NANO),1)
EXTLIBS   += -lc_nano -lg_nano 
else
EXTLIBS   += -lc -lgcc #-lc_nano -lg_nano #-lstdc++_nano -lsupc++_nano #-lm 
endif


LDSCRIPT  ?= $(TOPDIR)/chip/${CHIP}/arch/ram.ld


ifneq ($(strip $(CONFIG_EXT_RAM)),)
CFLAGS    += -DCONFIG_EXT_RAM
endif

ifneq ($(strip $(CONFIG_USE_RTT)),)
CFLAGS  += -DCONFIG_USE_RTT
CFLAGS  += -I $(TOPDIR)/modules/segger_rtt/include
MODULES += segger_rtt
LIBS    += -lsegger_rtt
endif

ifeq ($(CONFIG_SEGGER_SYSVIEW), 1)
CFLAGS  += -I $(TOPDIR)/modules/segger_rtt/include
endif

#
# following configuration items are always used in ATE TEST
#
# skip psram init step
ifneq ($(strip $(CONFIG_SKIP_PSRAM_INIT)),)
CFLAGS += -DCONFIG_SKIP_PSRAM_INIT=$(CONFIG_SKIP_PSRAM_INIT)
endif
# skip exception log print
ifneq ($(strip $(CONFIG_SKIP_EXCPT_LOG)),)
CFLAGS += -DCONFIG_SKIP_EXCPT_LOG=$(CONFIG_SKIP_EXCPT_LOG)
endif
# skip clocks (except PLL or Core related) init
ifneq ($(strip $(CONFIG_SKIP_EXTCLK_INIT)),)
CFLAGS += -DCONFIG_SKIP_EXTCLK_INIT=$(CONFIG_SKIP_EXTCLK_INIT)
endif
