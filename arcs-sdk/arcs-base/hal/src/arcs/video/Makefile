TOPDIR=../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		=
TARGET	= video

# modules depend by this module
MODULES = bsp driver segger_rtt ls_jpeg

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
#LIBS	= -lrtos -lutils -lbsp -lopi -lmva
#LIBS	= -lbsp  -llvgl -llvgl_demo_smartwatch
#LIBS	= -lbsp -llvgl_core -llvgl_draw -llvgl_extra -llvgl_font -llvgl_hal -llvgl_misc -llvgl_widgets \
          -llvgl_demo_benchmark -llvgl_demo_keypad -llvgl_demo_music -llvgl_demo_stress -llvgl_demo_widgets \
          -llvgl_demo_smartwatch -llvgl_demo_touch

LIBS	= -lbsp -lsegger_rtt -lls_jpeg

ifeq ($(IC_BOARD), 0)
LIBS	+= -ldrv_fpga
else
LIBS	+= -ldrv
endif

#exclude subdirs
exclude_dirs=$(patsubst ./%,%,$(shell find . -maxdepth 1 -type d))

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS  = $(shell find ./driver -name "*.c")
CSRCS += $(shell find ./device/camera -name "*.c")
CSRCS += $(shell find ./device/lcd -name "*.c")
#CSRCS += $(shell find ./device/touch -name "*.c")
#CSRCS += $(shell find ./middlewares/gui -name "*.c")
CSRCS += $(shell find ./middlewares/jpeg -name "*.c")
#CSRCS += $(shell find ./middlewares/lvgl -name "*.c")
CSRCS += $(shell find ./middlewares/conversions -name "*.c")
CSRCS += $(shell find ./middlewares/malloc -name "*.c")
#CSRCS += $(shell find ./middlewares/segger_rtt -name "*.c")
CSRCS += ./user/test_misc.c
#CSRCS += ./user/user_lcd.c
CSRCS += ./user/test_dvp.c
CSRCS += ./user/test_rgb.c
CSRCS += ./user/test_qspi_in.c
CSRCS += ./user/test_qspi_out.c
CSRCS += ./user/test_jpeg.c
CSRCS += ./user/test_blender.c
CSRCS += ./user/test_gpdma2d.c
CSRCS += ./user/test_pipe.c
CSRCS += ./user/test_2d.c
CSRCS += ./user/test_lvgl_dma2d_port.c
CSRCS += ./user/test_rgb888_rgb565.c
CSRCS += ./user/main.c

CPPSRCS	= 
SSRCS	= 

#includings and flags
CFLAGS = -I $(TOPDIR)/include/CMSIS \
         -I $(TOPDIR)/include/CMSIS/core \
         -I $(TOPDIR)/include/NMSIS/Core/Include \
		 -I $(TOPDIR)/include/driver \
		 -I $(TOPDIR)/include/bsp \
		 -I $(TOPDIR)/modules/segger_rtt/include \
		 -I $(TOPDIR)/include \
		 -I $(TOPDIR)/chip/${CHIP}/bsp \
		 -I $(TOPDIR)/chip/${CHIP}/include/register \
		 -I $(TOPDIR)/chip/${CHIP}/include \
		 -I $(TOPDIR)/chip/${CHIP}/ls_jpeg \
		 -I ./config \
		 -I ./driver \
		 -I ./driver/timer \
		 -I ./driver/gpio \
		 -I ./driver/sw_uart \
		 -I ./driver/sw_i2c \
		 -I ./driver/sw_spi \
		 -I ./driver/i2c \
		 -I ./driver/spi \
		 -I ./driver/qspi_lcd \
		 -I ./driver/qspi_in \
		 -I ./driver/dvp \
		 -I ./driver/rgb \
		 -I ./driver/clk_reset \
		 -I ./driver/lvgl_dma2d \
		 -I ./device/camera \
		 -I ./device/camera/sensors \
		 -I ./device/camera/sensors/inc \
		 -I ./device/lcd \
		 -I ./device/lcd/st7789 \
		 -I ./device/lcd/spd2010 \
		 -I ./device/lcd/axs15231b \
		 -I ./device/touch \
		 -I ./device/touch/ft5336 \
		 -I ./middlewares/gui \
		 -I ./middlewares/jpeg \
		 -I ./middlewares/conversions \
		 -I ./middlewares/malloc \
		 -I ./user


include $(TOPDIR)/rules.mk
#LDSCRIPT  = $(TOPDIR)/chip/${CHIP}/arch/ram.ld
LDSCRIPT  = ./ram.ld
