TOPDIR	= ../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB		= libcoreHTTP.a
TARGET	= 
LDS		= 

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
EXDIRS	=

#files want to build: eg. CSRCS=x.c  SSRCS=x.S
CSRCS	= $(shell find . -maxdepth 10 -type f -name "*.c" | grep -v test)
SSRCS	= 
# OPTIM	= -Os

#includings and flags
CFLAGS += -Dhttp_message_needs_eof=ls_http_message_needs_eof
CFLAGS += -Dhttp_should_keep_alive=ls_http_should_keep_alive
CFLAGS += -Dhttp_parser_execute=ls_http_parser_execute
CFLAGS += -Dhttp_method_str=ls_http_method_str
CFLAGS += -Dhttp_parser_init=ls_http_parser_init
CFLAGS += -Dhttp_errno_name=ls_http_errno_name
CFLAGS += -Dhttp_errno_description=ls_http_errno_description
CFLAGS += -Dhttp_parser_parse_url=ls_http_parser_parse_url
CFLAGS += -Dhttp_parser_pause=ls_http_parser_pause
CFLAGS += -Dhttp_body_is_final=ls_http_body_is_final
CFLAGS += -Dhttp_parser_set_max_header_size=ls_http_parser_set_max_header_size
CFLAGS += -Dhttp_parser_url_init=ls_http_parser_url_init
CFLAGS += -Dhttp_errno_description=ls_http_errno_description
CFLAGS += -Dhttp_errno_name=ls_http_errno_name
CFLAGS += -Dhttp_status_str=ls_http_status_str
CFLAGS += -Dhttp_parser_settings_init=ls_http_parser_settings_init
CFLAGS += -Dhttp_parser_version=ls_http_parser_version
CFLAGS += -DHTTP_DO_NOT_USE_CUSTOM_CONFIG
CFLAGS += -I./source/include
CFLAGS += -I./source/interface
CFLAGS += -I./source/dependency/3rdparty/http_parser
CFLAGS += -I$(TOPDIR)/include/lisa_porting/lisa_os
CFLAGS += -I$(TOPDIR)/include/lisa_porting/lisa_log \
			$(shell find $(TOPDIR)/include $(TOPDIR)/src -type d -exec echo "-I{}/ " \;)

include $(TOPDIR)/rules.mk
