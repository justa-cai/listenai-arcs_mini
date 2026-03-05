TOPDIR := ../../../

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB	= libmbedtls.a
TARGET  =

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
MBEDTLSDIR := $(MBEDTLS_VER)
PORT := rtos
CSRCS_CORE := $(MBEDTLSDIR)/library/platform_util.c \
              $(MBEDTLSDIR)/library/constant_time.c \
              $(MBEDTLSDIR)/library/bignum.c \
              $(MBEDTLSDIR)/library/ecp.c \
              $(MBEDTLSDIR)/library/ecp_curves.c \
              $(MBEDTLSDIR)/library/ecdsa.c \
              $(MBEDTLSDIR)/library/ecdh.c \
              $(MBEDTLSDIR)/library/pk.c \
              $(MBEDTLSDIR)/library/pk_wrap.c \
              $(MBEDTLSDIR)/library/pkparse.c \
              $(MBEDTLSDIR)/library/md.c \
              $(MBEDTLSDIR)/library/asn1parse.c \
              $(MBEDTLSDIR)/library/asn1write.c \
              $(MBEDTLSDIR)/library/oid.c
ifeq ($(strip $(MBEDTLS_MAJOR_VER)), 3)
CSRCS_CORE += $(MBEDTLSDIR)/library/bignum_core.c
endif

CSRCS_CORE += port/mbedtls_crypto.c

$(info mbedtls major ver $(MBEDTLS_MAJOR_VER))

CPPSRCS	=
SSRCS	=
CSRCS  := $(CSRCS_CORE)

#includings and flags
CFLAGS := -I$(MBEDTLSDIR)/include \
          -I$(MBEDTLSDIR) \
          -I$(TOPDIR)/include/rtos \
          -I$(TOPDIR)/chip/$(CHIP)/rtos/rtos_al \
          -I$(TOPDIR)/include/CMSIS \
          -I$(TOPDIR)/include/CMSIS/core \
          -I$(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure \
          -I$(TOPDIR)/chip/${CHIP}/mbedtls/port

CFLAGS += -DCFG_HSU
ifneq ($(strip $(MBEDTLS_MAJOR_VER)), "")
CFLAGS += -DMBEDTLS_MAJOR_VER=$(MBEDTLS_MAJOR_VER)
endif

#ext:
#	@touch version.c

include $(TOPDIR)/rules.mk
