/*
 * Copyright (c) 2024, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "ClockManager.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "usbh_core.h"
#include "usbh_serial.h"

#include "sys_init.h"

#define SERIAL_TEST_LEN (1 * 1024)

volatile uint32_t serial_tx_bytes = 0;
volatile uint32_t serial_rx_bytes = 0;
volatile bool serial_is_opened = false;
volatile bool serial_device_disconnected = false;  // 设备断开标志

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t serial_tx_buffer[SERIAL_TEST_LEN];
uint8_t serial_rx_data[SERIAL_TEST_LEN];

static void usbh_serial_thread(void *argument)
{
    int ret;
    struct usbh_serial *serial;
    bool serial_test_success = false;

    serial = usbh_serial_open("/dev/ttyACM0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
    if (serial == NULL) {
        serial = usbh_serial_open("/dev/ttyUSB0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
        if (serial == NULL) {
            printf("No serial device found\r\n");
            goto delete;
        }
    }

    printf("[INFO] Serial device opened successfully\r\n");
    
    struct usbh_serial_termios termios;

    memset(&termios, 0, sizeof(termios));
    termios.baudrate = 115200;
    termios.stopbits = 0;
    termios.parity = 0;
    termios.databits = 8;
    termios.rtscts = false;
    termios.rx_timeout = 0;
    
    printf("[INFO] Configuring serial port: 115200 8N1\r\n");
    ret = usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
    if (ret < 0) {
        printf("[ERROR] Set serial attr error, ret:%d\r\n", ret);
        goto delete_with_close;
    }
    
    printf("[INFO] Serial port configured, waiting for device ready...\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));  // 等待设备就绪

    // 生成递增测试数据（可检测顺序错误）
    printf("[INFO] Generating test pattern (incremental sequence 0x00-0xFF)...\r\n");
    for (uint32_t i = 0; i < sizeof(serial_tx_buffer); i++) {
        serial_tx_buffer[i] = i & 0xFF;
    }
    printf("[INFO] Start serial loopback test, len: %d\r\n", SERIAL_TEST_LEN);

    serial_tx_bytes = 0;
    while (1) {
        // 检查设备是否已断开
        if (serial_device_disconnected) {
            printf("[WARN] Device disconnected during transmission\r\n");
            goto delete_with_close;
        }
        
        ret = usbh_serial_write(serial, serial_tx_buffer + serial_tx_bytes, 
                                sizeof(serial_tx_buffer) - serial_tx_bytes);
        if (ret < 0) {
            printf("[ERROR] Serial write error, ret:%d (sent %d bytes)\r\n", ret, serial_tx_bytes);
            goto delete_with_close;
        } else if (ret == 0) {
            printf("[WARN] Serial write returned 0, retrying...\r\n");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        } else {
            serial_tx_bytes += ret;
            printf("[TX] Sent %d bytes, total: %d/%d\r\n", ret, serial_tx_bytes, SERIAL_TEST_LEN);

            if (serial_tx_bytes >= SERIAL_TEST_LEN) {
                printf("[INFO] Send over\r\n");
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // 避免发送过快
    }

    printf("[INFO] Waiting for loopback data...\r\n");
    serial_rx_bytes = 0;
    int timeout = 500;
    while (1) {
        // 检查设备是否已断开
        if (serial_device_disconnected) {
            printf("[WARN] Device disconnected during reception\r\n");
            goto delete_with_close;
        }
        
        ret = usbh_serial_read(serial, serial_rx_data + serial_rx_bytes, SERIAL_TEST_LEN - serial_rx_bytes);
        if (ret > 0) {
            serial_rx_bytes += ret;
            printf("[RX] Received %d bytes, total: %d/%d\r\n", ret, serial_rx_bytes, SERIAL_TEST_LEN);
            if (serial_rx_bytes >= SERIAL_TEST_LEN) {
                printf("[INFO] Receive over\r\n");
                break;
            }
            timeout = 500;  // 重置超时计数
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
            if (timeout == 0) {
                printf("[ERROR] Serial read timeout (received %d/%d bytes)\r\n", serial_rx_bytes, SERIAL_TEST_LEN);
                goto delete_with_close;
            }
        }
    }

    printf("[INFO] Verifying loopback data...\r\n");
    uint32_t error_count = 0;
    for (uint32_t i = 0; i < SERIAL_TEST_LEN; i++) {
        uint8_t expected = i & 0xFF;
        if (serial_rx_data[i] != expected) {
            if (error_count < 10) {  // 只打印前 10 个错误，避免日志刷屏
                printf("[ERROR] Data mismatch at index %d: expected 0x%02x, got 0x%02x\r\n", 
                       i, expected, serial_rx_data[i]);
            }
            error_count++;
        }
    }
    
    if (error_count > 0) {
        printf("[ERROR] Total %d bytes mismatched out of %d (%.2f%% error rate)\r\n", 
               error_count, SERIAL_TEST_LEN, (error_count * 100.0) / SERIAL_TEST_LEN);
        goto delete_with_close;
    }
    
    serial_test_success = true;
    printf("[SUCCESS] All %d bytes verified correctly! (0 errors)\r\n", SERIAL_TEST_LEN);

delete_with_close:
    if (serial_test_success) {
        printf("\r\n========================================\r\n");
        printf("  Serial Loopback Test: SUCCESS ✓\r\n");
        printf("========================================\r\n");
    } else {
        printf("\r\n========================================\r\n");
        printf("  Serial Loopback Test: FAILED ✗\r\n");
        printf("========================================\r\n");
    }
    
    printf("[INFO] Closing serial device...\r\n");
    
    // 关闭串口设备
    if (serial != NULL) {
        usbh_serial_close(serial);
        serial = NULL;
    }
    
    printf("[INFO] Test completed, thread waiting for device disconnect...\r\n");
    
delete:
    // 不删除线程，而是进入等待状态，直到设备断开
    while (!serial_device_disconnected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    printf("[INFO] Device disconnected, cleaning up...\r\n");
    serial_is_opened = false;
    
    // 等待一下确保 USB 框架处理完成
    vTaskDelay(pdMS_TO_TICKS(200));
    
    printf("[INFO] Serial thread exiting safely\r\n");
    vTaskDelete(NULL);
}

void usbh_serial_run(struct usbh_serial *serial)
{
    if (serial_is_opened) {
        printf("[WARN] Serial thread already running, ignoring...\r\n");
        return;
    }
    serial_is_opened = true;
    serial_device_disconnected = false;  // 重置断开标志
    printf("[INFO] Creating serial thread...\r\n");
    xTaskCreate(usbh_serial_thread, "usbh_serial", 2048, serial, CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}

void usbh_serial_stop(struct usbh_serial *serial)
{
    printf("[INFO] Stopping serial thread (device disconnected)...\r\n");
    serial_device_disconnected = true;  // 设置断开标志
    serial_is_opened = false;
    // 给线程一些时间安全退出
    vTaskDelay(pdMS_TO_TICKS(200));
    printf("[INFO] Serial thread stopped\r\n");
}

static void usbh_event_handler(uint8_t busid, uint8_t hub_index, uint8_t hub_port, uint8_t intf, uint8_t event)
{
    const char *event_str[] = {
        "UNKNOWN",
        "CONNECTED",
        "DISCONNECTED",
        "REMOVED"
    };
    
    printf("\r\n[USB Event] busid=%d, hub_index=%d, hub_port=%d, intf=%d, event=%d (%s)\r\n", 
           busid, hub_index, hub_port, intf, event,
           (event < 4) ? event_str[event] : "INVALID");
}

int main(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  CherryUSB Host Serial Example\r\n");
    printf("========================================\r\n");
    printf("\r\n");
    
    printf("[INFO] Initializing USB Host...\r\n");
    usbh_initialize(0, 0x41000000UL, usbh_event_handler);
    printf("[INFO] USB Host initialized, waiting for device...\r\n");
    printf("[INFO] Please connect USB Serial device\r\n");
    printf("[INFO] Supported: CDC ACM (tested), CH340/CP2102/FT232/PL2303 (untested)\r\n");
    printf("\r\n");

    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        count++;
        printf("[%d] Still waiting for USB device... (check cable and device)\r\n", count);
    }
    
    return 0;
}

static int usb_host_init(void)
{
    printf("[INIT] Configuring USB PHY for Host mode...\r\n");
    
    // 配置为 Host 模式
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;      // Host 模式 (ID=0)
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;   // 16位模式

    printf("[INIT] USB Host PHY configured:\r\n");
    printf("       - IDDIG = 0 (Host mode)\r\n");
    printf("       - Clock enabled\r\n");
    printf("       - 16-bit data bus\r\n");
    return 0;
}

SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
