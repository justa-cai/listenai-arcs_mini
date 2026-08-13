# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

config BOOT_AP_ENTRY
	hex "AP entry"
	default 0

config BOOT_CP_ENTRY
	hex "CP entry"
	default 0
	depends on !MEM_CONFIG

config BOOT_FLASH_SIZE
	hex "Flash size"
	default 0x4000

choice
	prompt "boot log level"
	default BOOT_LOG_LVL_ERR

	config BOOT_LOG_LVL_NON
		bool "non"
	config BOOT_LOG_LVL_ERR
		bool "err"
	config BOOT_LOG_LVL_WRN
		bool "wrn"
	config BOOT_LOG_LVL_INF
		bool "inf"
	config BOOT_LOG_LVL_DBG
		bool "dbg"

endchoice

config BOOT_UART_PORT
	int "boot log port"
	default 0

config BOOT_UART_TX_PIN
	int "boot uart tx pin"
	default 3

config BOOT_UART_BAUDRATE
	int "boot log baud rate"
	default 921600

config BOOT_UART_PIN_FUNC_SEL
	int "boot log pin func sel"
	default 2
