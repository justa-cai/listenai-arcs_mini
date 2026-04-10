/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG    "main"
#if CONFIG_LOG
#include "lisa_log.h"
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
#include "ClockManager.h"
#include "log_print.h"
#include "tusb.h"
#include "arcs_ap.h"
#include "disk/disk_access.h"
#include <disk/disk.h>
#include "lisa_device.h"
#include "lisa_sdmmc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAIN_TASK_STACK_SIZE 1024
#define MAIN_TASK_PRIORITY   configMAX_PRIORITIES - 1

#define USBD_STACK_SIZE (3*configMINIMAL_STACK_SIZE)

// Block size (in bytes)
#define DISK_BLOCK_SIZE    512

// Whether the disk is ejected
static bool ejected = false;

// SCSI Commands that are not in tusb_msc.h
#define SCSI_CMD_MODE_SENSE_10 0x5A

// Invoked when device is mounted
void tud_mount_cb(void)
{
    LOGI("[%s] Device mounted", __func__);
    ejected = false;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    LOGI("[%s] Device unmounted", __func__);
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    LOGI("[%s] Device suspended", __func__);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    LOGD("[%s] Device resumed", __func__);
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void) lun;

    const char vid[] = "TinyUSB";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    memcpy(vendor_id  , vid, strlen(vid));
    memcpy(product_id , pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}



// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void) lun;

    // RAM disk is ready until ejected
    if (ejected) {
        // Additional Sense 3A-00 is NOT_FOUND
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    (void) lun;

    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    uint32_t sector_count = 0;
    uint32_t sector_size = 0;

    // Get disk capacity
    if (disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count) != 0 ||
        disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size) != 0) {
        // If failed to get disk info, use default values
        sector_count = 0x0001000; // 4MB
        sector_size = DISK_BLOCK_SIZE;
    }

    *block_count = sector_count;
    *block_size  = sector_size;

    LOGD("[%s] block_count: %lu, block_size: %u", __func__, *block_count, *block_size);
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void) lun;
    (void) power_condition;

    LOGD("[%s] power:%d, start:%d, load_eject:%d", __func__, power_condition, start, load_eject);

    if (load_eject)
    {
        if (!start)  // Eject
        {
            const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
            int status = disk_access_status(pdrv);
            LOGD("[%s] disk status before eject: %d", __func__, status);

            if (status == DISK_STATUS_OK)
            {
                ejected = true;
                LOGD("[%s] disk ejected", __func__);
            }
            else
            {
                LOGD("[%s] disk not ready for eject", __func__);
                return false;
            }
        }
        else  // Load
        {
            ejected = false;
            LOGD("[%s] disk loaded", __func__);
        }
    }
    else  // Start/Stop power
    {
        if (!start)
        {
            LOGD("[%s] stopped power mode", __func__);
        }
        else
        {
            LOGD("[%s] active mode", __func__);
        }
    }

    return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    (void) lun;

    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;

    int status = disk_access_status(pdrv);
    
    if (status != DISK_STATUS_OK) {
        LOGD("[%s] disk not ready", __func__);
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return -1;
    }

    int ret = disk_access_read(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE);
    if (ret != 0) {
        LOGD("[%s] disk_access_read failed with error %d", __func__, ret);
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00);
        return -1;
    }

    return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    (void) lun;
    // LOGD("[%s]: lba:%lu, offset:%lu, bufsize:%lu", __func__, lba, offset, bufsize);

    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    
    int status = disk_access_status(pdrv);
    // LOGD("[%s] disk status: %d", __func__, status);
    
    if (status != DISK_STATUS_OK) {
        LOGD("[%s] disk not ready", __func__);
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return -1;
    }

    int ret = disk_access_write(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE);
    if (ret != 0) {
        LOGD("[%s] disk_access_write failed with error %d", __func__, ret);
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00);
        return -1;
    }

    return bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun)
{
    (void) lun;
    bool writable = true;

    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    int status = disk_access_status(pdrv);

    if (status == DISK_STATUS_OK) {
        writable = true;
    } else {
        writable = false;
    }
    LOGD("[%s] writable: %d", __func__, writable);

    return writable;
}


// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
// read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) )
  {
    if(in_xfer)
    {
      memcpy(buffer, response, (size_t) resplen);
    }else
    {
      // SCSI output
    }
  }

  return (int32_t) resplen;
}

// USB Device Driver task
static void usb_device_task(void *param)
{
    (void) param;

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    // RTOS forever loop
    while (1)
    {
        tud_task();
    }
}

static void user_usbd_msc_init(void)
{
    //enable usb clock
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; //16bit mode

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host

    LOGD("TinyUSB MSC class ready\n");
}

static int user_disk_init(void)
{
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));

    disk_init(NULL);

    return 0;
}


int main(int argc, char **argv)
{
    int ret = 0;
#if CFG_TUSB_OS == OPT_OS_FREERTOS
    
    LOGI("Start usb device msc sample\n");

    ret = user_disk_init();
    if(ret < 0){
        LOGE("user_disk_init failed");
        return ret;
    }

    LOGI("user_disk_init success");

    user_usbd_msc_init();

    xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

#else
    lisa_log_init();
    
    log_i("Start usb device msc sample\n");

    user_usbd_msc_init();

    while (1) {
        tud_task(); // tinyusb device task
    }
#endif

    return 0;
}
