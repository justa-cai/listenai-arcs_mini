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

config BOOT_UART_PORT
    int "boot log port"
    default SYSLOG_UART_PORT if BOOT
    default 0

config BOOT_UART_TX_PIN
    int "boot uart tx pin"
    default SYSLOG_UART_TX_PIN if BOOT
    default 3

config BOOT_UART_BAUDRATE
    int "boot log baud rate"
    default SYSLOG_UART_BAUDRATE if BOOT
    default 921600

config BOOT_UART_PIN_FUNC_SEL
    int "boot log pin func sel"
    default SYSLOG_UART_TX_PIN_FUNC_SEL if BOOT
    default 2

endchoice
