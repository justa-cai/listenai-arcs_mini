
WIFI_CONF_HEADER := $(TOPDIR)/chip/${CHIP}/include/wifi/ls_wifi_type.h

SDK_VER = 0.0.0

# shell env (sh)
SHELL_CMD = /bin/sh -c
DATE_CMD = date +"%b %d %Y %H:%M:%S"
USER_CMD = whoami
GIT_CMD = git rev-parse --short HEAD

# Rule to generate version header file
pre:
	@echo "Generating version header file..."
	@echo "// Generated file, do not edit." > $(SDK_HEADER_FILE)
	@echo "#define SDK_VER_ON 1" >> $(SDK_HEADER_FILE)
	@echo "#define SDK_BUILD_VER \"$(SDK_VER)\"" >> $(SDK_HEADER_FILE)
	@echo "#define SDK_BUILD_DATE \"$(shell $(DATE_CMD))\"" >> $(SDK_HEADER_FILE)
	@echo "#define SDK_BUILD_USER \"$(shell $(USER_CMD))\"" >> $(SDK_HEADER_FILE)
	@echo "#define SDK_COMMIT_ID \"$(shell $(GIT_CMD))\"" >> $(SDK_HEADER_FILE)
	@echo "Header file $(SDK_HEADER_FILE) created."
ifdef VIF_NUM
	@if grep -qE '^\s*#\s*define\s+VIF_MAX\b' $(WIFI_CONF_HEADER); then \
		sed -i 's|^\s*#\s*define\s\+VIF_MAX\b.*|#define VIF_MAX ($(VIF_NUM))|' $(WIFI_CONF_HEADER); \
	else \
		sed -i "/MAX_AP_SCAN/i #define VIF_MAX ($(VIF_NUM))\n" $(WIFI_CONF_HEADER); \
	fi
endif
