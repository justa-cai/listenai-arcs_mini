###########  rules.mk  ##########
# Top level pattern, include by Makefile of child directory
# in which variable like TOPDIR, TARGET or LIB may be needed

HOST_OS := $(shell uname -o)

ifneq ($(strip $(CHIP)),)
include $(TOPDIR)/chip/${CHIP}/arch/platform.mk
endif


ifneq ($(strip $(CHIP)), x64)
ifeq ("$(CROSS_COMPILE)", "")
$(error PLEASE SPECIFY THE TOOLCHAIN PATH FIRST!!!)
endif
endif


CC	           = $(CROSS_COMPILE)gcc
C++	           = $(CROSS_COMPILE)g++
CPP	           = $(CROSS_COMPILE)cpp -E -P -undef
OBJCOPY	       = $(CROSS_COMPILE)objcopy
AR	           = $(CROSS_COMPILE)ar    #gcc-ar
AS	           = $(CROSS_COMPILE)as
NM             = $(CROSS_COMPILE)nm    #gcc-nm
READELF        = $(CROSS_COMPILE)readelf
STRIP          = $(CROSS_COMPILE)strip
RM             = -rm -rf
MAKE           = make

#defines
PLATFORM_DEF   =
OS_DEF         =

ifeq ($(strip $(IC_BOARD)),)
IC_BOARD = 1
endif

ifeq ($(strip $(CHIP)), venus)
CFLAGS += -DCSK6001
else
endif

ifeq ($(strip $(CHIP)), arcs)
CFLAGS += -DUNITY_INCLUDE_CONFIG_H
else
endif

ifeq ($(strip $(SDK_VER)),)
SDK_VER = 0.0.0
endif

ifneq ($(strip $(TARGET)),)
override TGT := $(TARGET)
endif

ifeq ($(strip $(PSRAM_SEC)),)
PSRAM_SEC = 0
endif

ifeq ($(strip $(BUILD_ROM)),)
BUILD_ROM = 0
endif


ifeq ("${CHIP}", "venus")
BUILDTOOL = ARM_CM33_NTZ
else ifeq ("${CHIP}", "arcs")
BUILDTOOL = riscv_nuclei
else ifeq ("${CHIP}", "mars")
BUILDTOOL = riscv_nuclei
else ifeq ("${CHIP}", "vegah")
BUILDTOOL = riscv_nuclei
else ifeq ("${CHIP}", "apus")
BUILDTOOL = riscv_nuclei
else ifeq ("${CHIP}", "jupiter")
BUILDTOOL = riscv_nuclei
else ifeq ("${CHIP}", "venusa")
BUILDTOOL = riscv_nuclei
else
$(info "No BUILDTOOL for" $(CHIP) $(TGT))
endif
export BUILDTOOL


ifeq (${CFG_RTOS},1)
OS_DEF = -DCFG_RTOS
endif

MKDEFS= CHIP=${CHIP} TGT=${TGT} IC_BOARD=${IC_BOARD} CONFIG_TRACE=${CONFIG_TRACE} BUILD_NANO=${BUILD_NANO} BUILD_ROM=${BUILD_ROM} SDK_VER=$(SDK_VER) PSRAM_SEC=$(PSRAM_SEC) WIFI_TARGET=$(WIFI_TARGET)

ifeq (${BUILD_NANO},1)
OPTIM     = -Os
else
OPTIM     = -O2    # -flto -ffat-lto-objects
endif


ifeq ($(BUILD_ROM),1)
LDSCRIPT   ?= $(TOPDIR)/chip/${CHIP}/arch/rom.ld
else
LDSCRIPT   ?= $(TOPDIR)/chip/${CHIP}/arch/flash.ld
endif

AFLAGS    += #-mthumb -gdwarf-2 -mthumb-interwork
#move to platfrom.mk

CFLAGS    += \
	$(PLATFORM_DEF) \
	$(OS_DEF) \
	-DIC_BOARD=$(IC_BOARD) \
	-DCHIP=${CHIP} \
	-DTGT=${TGT} \
	-DBUILD_INNER=${INNER} \
	-DBUILD_ROM=${BUILD_ROM} \
	-DPSRAM_SEC=${PSRAM_SEC} \
	$(COREFLAGS) \
	$(OPTIM) -g \
	-Wall -Wno-format -Wno-unused -Wno-comment -MMD \
	-ffunction-sections -fdata-sections \


LDFLAGS   += \
	$(COREFLAGS) \
	-T $(LIBOUT)/$(CHIP).ld \


dirs      := $(shell find . -maxdepth 1 -type d)
dirs      := $(basename $(patsubst ./%,%,$(dirs)))
dirs      := $(filter-out $(exclude_dirs),$(dirs))
SUBDIRS   := $(dirs)

COBJS      = $(CSRCS:.c=.o)
CPPOBJS    = $(CPPSRCS:.cc=.o)
SOBJS      = $(SSRCS:.S=.o)
DEPS       = $(CSRCS:.c=.d)
DEPS      += $(CPPSRCS:.cc=.d)
DEPS      += $(SSRCS:.S=.d)
DEPS_PATH  = ${foreach dep,$(DEPS),$(OBJPATH)/$(dep)}  

OBJS_NAME  = $(COBJS) $(CPPOBJS) $(SOBJS)
OBJS       = ${foreach obj,$(OBJS_NAME),$(OBJPATH)/$(obj)}  
OBJS_DIRS  = $(sort $(foreach obj, $(OBJS), $(shell dirname $(obj))))
#DEP_LIBS   = $(filter $(wildcard $(LIBOUT)/*.a), $(patsubst -l%, $(LIBOUT)/lib%.a, $(LIBS)))
DEP_LIBS   = $(patsubst -l%, $(LIBOUT)/lib%.a, $(LIBS))

TGTOUT     = $(TOPDIR)/out/${CHIP}
LIBOUT     = $(TOPDIR)/lib/${CHIP}-${TGT}
LDOUT      = $(notdir $(LDSCRIPT))
OBJPATH    = $(LIBOUT)/$(basename $(LIB))$(TARGET)

LIBSGROUP := -Wl,--start-group $(LIBS) ${EXTLIBS} -Wl,--end-group -L${LIBOUT}

.PHONY: all clean ld libs apps subdirs_l subdirs_t subdirs_c pre ext strip show_lib_ver mk_libs

all     : libs apps
libs    : pre $(LIBOUT)/$(LIB) ext subdirs_l
apps    : pre $(TGTOUT)/$(TARGET) ext subdirs_t

$(LIBOUT)/$(LIB)   : mk_dirs $(OBJS)
	if [ "$(LIB)" != "" ]; then \
	mkdir -p $(LIBOUT);	\
	$(AR) rs $@  $(OBJS); \
	fi

mk_libs :
	@for dir in $(MODULES); do \
	if [ -f "${TOPDIR}/$$dir/Makefile" ]; then $(MAKE) -C ${TOPDIR}/$$dir $(MKDEFS) libs ||exit 1; fi;\
	if [ -f "${TOPDIR}/modules/$$dir/Makefile" ]; then $(MAKE) -C ${TOPDIR}/modules/$$dir $(MKDEFS) libs ||exit 1; fi;\
	if [ -f "${TOPDIR}/chip/${CHIP}/$$dir/Makefile" ]; then $(MAKE) -C ${TOPDIR}/chip/${CHIP}/$$dir $(MKDEFS) libs ||exit 1; fi;\
	done

$(TGTOUT)/$(TARGET): mk_dirs mk_libs $(DEP_LIBS) $(OBJS) $(LDSCRIPT) 
	if [ "$(TARGET)" != "" ]; then \
	$(CPP) $(LDSCRIPT) $(CFLAGS) -o $(LIBOUT)/$(CHIP).ld; \
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBSGROUP); \
	$(NM) -n -l -C $@ > $@.symbol; \
	$(READELF) -a  $@ > $@.readelf; \
	$(OBJCOPY) -S -O binary $@ $@.bin; \
	$(CROSS_COMPILE)objdump -S $@ > $@.dis; \
	$(CROSS_COMPILE)size $@; \
	$(if $(filter-out 0,$(CONFIG_TRACE)),\
		echo ====== Copy this address into Tracealyzer '>' PSF Streaming Settings '>' RTT Control Block Address ===== \
		&& grep _SEGGER_RTT $@.symbol \
		&& echo ====================================================================================================;) \
	fi


mk_dirs :
	mkdir -p $(TGTOUT)
	if [ "$(OBJS_DIRS)" != "" ]; then \
	mkdir -p $(OBJS_DIRS); \
	fi

mkhdr   :
	$(TOPDIR)/tools/mkhdr.sh $(TGTOUT)/$(TARGET).bin

subdirs_l	:
	@for dir in $(SUBDIRS);\
	do if [ -f "$$dir/Makefile" ]; then $(MAKE) -C $$dir $(MKDEFS) libs||exit 1; fi;\
	done

subdirs_t	:
	@for dir in $(SUBDIRS);\
	do if [ -f "$$dir/Makefile" ]; then $(MAKE) -C $$dir $(MKDEFS) apps||exit 1; fi;\
	done

${OBJPATH}/%.o: %.S
	$(CC) -c $(CFLAGS) $(AFLAGS) $< -o $@

${OBJPATH}/%.o: %.c
	$(CC) -c $(CFLAGS) -std=gnu11 $< -o $@

${OBJPATH}/%.o: %.cc
	$(C++) -c $(CFLAGS) -std=gnu++17 $< -o $@

subdirs_c	:
	@for dir in $(SUBDIRS);\
	do if [ -f "$$dir/Makefile" ]; then $(MAKE) -C $$dir IC_BOARD=$(IC_BOARD) clean||exit 1; fi;\
	done

clean	: subdirs_c
	@if [ -n "$(TARGET)" ]; then rm -rf  $(LIBOUT)/* $(TGTOUT)/$(TARGET) $(TGTOUT)/$(TARGET).* $(OBJS) ; fi
	@if [ -n "$(TARGET)" ]; then rm -rf  $(DEPS_PATH) *.o; fi
	@if [ -n "$(LIB)" ]; then rm -rf $(LIBOUT)/* $(OBJS); fi
	@if [ -n "$(LIB)" ]; then rm -rf $(DEPS_PATH) *.o; fi
	@for dir in $(MODULES);\
	do if [ -f "${TOPDIR}/$$dir/Makefile" ]; then $(MAKE) -C ${TOPDIR}/$$dir $(MKDEFS) clean ||exit 1; fi;\
	done

strip:
	for liba in $(shell find $(LIBOUT) -name "*.a" -type f);\
	do $(STRIP) --strip-debug $$liba;\
	done

-include $(DEPS_PATH)
