/**
 ****************************************************************************************
 * @file memdump.c
 *
 * @brief  Memory dump
 *
 * Copyright (C) Listenai 2024
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup MEMDUMP
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "memdump.h"
#include "Driver_UART.h"
#include "spiflash.h"
#include "log_print.h"
#include "dma.h"
#include "uart.h"

/*
 * MACROS
 ****************************************************************************************
 */
//#define hal_SendByte(uart, byte_to_send)      uart->REG_RXTX_BUFFER.all = byte_to_send;
//#define hal_GetByte(uart)                     uart->REG_RXTX_BUFFER.all
#define CHECK_RESOURCES(res)  do {\
        if ((res != UART0()) && (res != UART1())) {\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
} while(0)
/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
static uint8_t msg_buf[16];
const MDUMP_ENTRY mdump_table[] = {
        // cp ram
        {CMN_RAM0_REGION, FLASH_MDUMP_BASE, CMN_RAM0_REGION_SIZE},
        // ap ram
        {CMN_RAM1_REGION, FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE, CMN_RAM1_REGION_SIZE},
        // wifi shared ram
        {WIFI_RAM_REGION, FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE, WIFI_RAM_REGION_SIZE},
        // register
        {WIFI_MAC_CORE_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE,
         WIFI_MAC_CORE_SIZE},
        {WIFI_MAC_PL_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE,
         WIFI_MAC_PL_SIZE},
        {NEW_DFE_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE
         +WIFI_MAC_PL_SIZE,
         NEW_DFE_SIZE},
        {WIFI_MDMCFG_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE
         +WIFI_MAC_PL_SIZE+NEW_DFE_SIZE,
         WIFI_MDMCFG_SIZE},
        {RF_IF_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE
         +WIFI_MAC_PL_SIZE+NEW_DFE_SIZE+WIFI_MDMCFG_SIZE,
         RF_IF_SIZE},
        {WIFI_MACBYPASS_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE
         +WIFI_MAC_PL_SIZE+NEW_DFE_SIZE+WIFI_MDMCFG_SIZE+RF_IF_SIZE,
         WIFI_MACBYPASS_SIZE},
        {WF_CTRL_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE
         +WIFI_MAC_PL_SIZE+NEW_DFE_SIZE+WIFI_MDMCFG_SIZE+RF_IF_SIZE+WIFI_MACBYPASS_SIZE,
         WF_CTRL_SIZE},
        {WIFI_CRM_BASE,
         FLASH_MDUMP_BASE+CMN_RAM0_REGION_SIZE+CMN_RAM1_REGION_SIZE+WIFI_RAM_REGION_SIZE+WIFI_MAC_CORE_SIZE
         +WIFI_MAC_PL_SIZE+NEW_DFE_SIZE+WIFI_MDMCFG_SIZE+RF_IF_SIZE+WIFI_MACBYPASS_SIZE+WF_CTRL_SIZE,
         WIFI_CRM_SIZE}
};

static FLASH_DEV mdump_flash = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 2,
    .sclk_div = 0, // 0 means divider=2 //0xff,  //0xff means divider=1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000
};
/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

static void uart_read_msg_poll(UART_RESOURCES *uart, void *data, uint32_t num)
{
    uint32_t val;
    uint32_t i;
    if ((data == NULL) || (num == 0U)) {
        // Invalid parameters
        return;
    }

    // Save number of data to be received
    uart->info->xfer.rx_num = num;

    // Clear RX statuses
    uart->info->rx_status.rx_break = 0U;
    uart->info->rx_status.rx_framing_error = 0U;
    uart->info->rx_status.rx_overflow = 0U;
    uart->info->rx_status.rx_parity_error = 0U;

    // Save receive buffer info
    uart->info->xfer.rx_buf = (uint8_t *) data;
    uart->info->xfer.rx_cnt = 0U;

    while(uart->info->xfer.rx_cnt !=
            uart->info->xfer.rx_num) {
        val = uart->reg->REG_STATUS.bit.RX_FIFO_LEVEL;
        if(val) {
            if(val + uart->info->xfer.rx_cnt < uart->info->xfer.rx_num) {
                val += uart->info->xfer.rx_cnt;
            } else {
                val = uart->info->xfer.rx_num;
            }
            for (i = uart->info->xfer.rx_cnt; i < val; i++) {
                uart->info->xfer.rx_buf[i] = (uint8_t)hal_GetByte(uart->reg);
            }
            uart->info->xfer.rx_cnt = i;
        }
    }
}

static void uart_send_msg_poll(UART_RESOURCES *uart, void *data, uint32_t num)
{
    uint32_t val;
    if ((data == NULL) || (num == 0U)) {
        // Invalid parameters
        return;
    }
    // Save transmit buffer info
    uart->info->xfer.tx_buf = (uint8_t *) data;
    uart->info->xfer.tx_num = num;
    uart->info->xfer.tx_cnt = 0U;
    while (uart->reg->REG_STATUS.bit.TX_FIFO_SPACE < num);
    // Fill TX FIFO
    val = uart->reg->REG_STATUS.bit.TX_FIFO_SPACE;
    if (val) {
        while ((val--)&& (uart->info->xfer.tx_cnt != uart->info->xfer.tx_num)) {
            hal_SendByte(uart->reg, uart->info->xfer.tx_buf[uart->info->xfer.tx_cnt]);
            uart->info->xfer.tx_cnt++;
        }
    }
}

static uint16_t calculate_checksum(uint16_t *data, uint16_t byte_length)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < byte_length; i++) {
        sum += data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

static bool verify_command_checksum(uint8_t *data, uint16_t length, uint16_t checksum)
{
    uint16_t calculated_checksum = calculate_checksum(data, length);
    return calculated_checksum == checksum;
}

static int32_t memory_dump_daemon(void *res)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;
    uint32_t val;
    uint32_t start_address;
    uint16_t length, checksum;
    uint8_t *dump_data;

    while (1) {
        uart_read_msg_poll(uart, msg_buf, 12);  // Get msg from host by polling

        val = *(uint32_t *)&uart->info->xfer.rx_buf[0];
        if (val == MAGIC_PATTERN) {
            // Parse start addr, length and checksum
            start_address = *(uint32_t *)&uart->info->xfer.rx_buf[4];
            checksum = (*(uint32_t *)&uart->info->xfer.rx_buf[8]) >> 16;
            length = (*(uint32_t *)&uart->info->xfer.rx_buf[8]) & 0xFFFF;

            if (verify_command_checksum((uint8_t *)&uart->info->xfer.rx_buf[0], (10 >> 1), checksum)) {
                dump_data = (uint8_t *)start_address;

                uint16_t bundle =  1 << BUNDLE_SIZE_IN_LOG2;
                for (uint32_t i = 0; i < length; i += bundle) {
                    uart_send_msg_poll(uart, &dump_data[i], bundle);
                }

                uint16_t calculated_checksum = calculate_checksum(dump_data, (length >> 1));
                uart_send_msg_poll(uart, &calculated_checksum, 2);
            }
        }
    }
    return 0;
}

_EXT_RAM static int flash_write_mem_dump(MDUMP_ENTRY *entry)
{
    int ret = -1;
    flash_write_protection_set(&mdump_flash, false);
    ret = flash_erase(&mdump_flash, entry->dst, entry->len);
    if(ret != 0)
        CLOGD("Failed to erase flash, ret = %d\n", ret);
    ret = flash_write(&mdump_flash, entry->dst, entry->src, entry->len);
    if(ret != 0)
        CLOGD("Failed to write flash, ret = %d\n", ret);
    flash_write_protection_set(&mdump_flash, true);
    CLOGD("dump %d bytes from 0x%x to 0x%x done\n", entry->len, entry->src, entry->dst);

    return ret;
}

static int flash_mem_dump_all()
{
    int i;
    int num = sizeof(mdump_table)/sizeof(MDUMP_ENTRY);

    CLOGD("flash memdump %d sections, please wait...\n", num);
    for(i = 0; i < num; i++) {
        flash_write_mem_dump(&mdump_table[i]);
    }
    CLOGD("flash memdump done\n");
}

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

int32_t memdump_process(MDUMP_PATH path)
{
    switch (path)
    {
        case MDUMP_PATH_FLASH:
             flash_mem_dump_all();
             /* remove the break intentionally */
        case MDUMP_PATH_UART:
             disable_GINT();
             /* Delay several ticks before we increase the baudrate */
             volatile uint32_t delay = 100000;
             while(delay-- > 0);
             logInit(0, 1000000);
             memory_dump_daemon(UART0());
             break;
        case MDUMP_PATH_SDIO:

             break;
        case MDUMP_PATH_USB:

             break;
        default:
             break;
    }
}

/// @} MEMDUMP



