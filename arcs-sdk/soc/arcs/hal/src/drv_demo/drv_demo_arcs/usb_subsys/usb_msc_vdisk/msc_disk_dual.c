/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "main.h"
#include "tusb.h"
#include <string.h>

#if CFG_TUD_MSC

#define DEBUG_LOG   1
#if DEBUG_LOG
#define LOGD(fmt, ...)   CLOGD(fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)   ((void)0)
#endif // DEBUG_LOG

// Use internal PSRAM/SRAM memory to simulate disk0 (mass storage disk)!
#define __PSRAM_BASE    0x28000000 // PSRAM Start Address

#define __PSRAM_SIZE    0x00800000 // PSRAM SIZE: 8MB

#if MSC_USER_EP_BUF
CFG_TUSB_MEM_ALIGN static uint8_t my_mscd_buf[CFG_TUD_MSC_EP_BUFSIZE];
#endif


/*
#if MSC_USER_EP_BUF
#define __PSRAM_SIZE    (0x00800000 - CFG_TUD_MSC_EP_BUFSIZE)
static uint8_t *my_mscd_buf = (uint8_t *)(__PSRAM_BASE + __PSRAM_SIZE);
#else
#define __PSRAM_SIZE    0x00800000 // PSRAM SIZE: 8MB
#endif
*/


enum
{
  DISK0_BLOCK_NUM  = __PSRAM_SIZE / 512, // 8KB is the smallest size that windows allow to mount
  DISK1_BLOCK_NUM  = 128, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

/*
enum
{
  DISK0_HEAD_BLOCK_NUM  = 16, // 8KB for HEAD sectors
  DISK0_BLOCK_NUM  = 1024 * 1024 * 2, // 8KB is the smallest size that windows allow to mount
  DISK1_HEAD_BLOCK_NUM  = 16, // 8KB for HEAD sectors
  DISK1_BLOCK_NUM  = 1024 * 1024 * 8, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

#define DISK0_LABEL     \
    'B' , 'S' , 'D' , ' ' , 'L' , 'S' , 'A' , 'I' , '0' , '0' , '0'

#define DISK1_LABEL     \
    'B' , 'S' , 'D' , ' ' , 'L' , 'S' , 'A' , 'I' , '0' , '0' , '1'

#define BYTE0(x)    ((x) & 0xFF)
#define BYTE1(x)    (((x) >> 8) & 0xFF)
#define BYTE2(x)    (((x) >> 16) & 0xFF)
#define BYTE3(x)    (((x) >> 24) & 0xFF)
*/


//--------------------------------------------------------------------+
// LUN 0
//--------------------------------------------------------------------+

//uint8_t msc_disk0[DISK0_HEAD_BLOCK_NUM][DISK_BLOCK_SIZE] = { 0 };
uint8_t *msc_disk0_base = (uint8_t*)__PSRAM_BASE;

//--------------------------------------------------------------------+
// LUN 1
//--------------------------------------------------------------------+

uint8_t msc_disk1[DISK1_BLOCK_NUM][DISK_BLOCK_SIZE] = { 0 };


#if MSC_USER_EP_BUF
//BSD: set EP OUT buffer and its length, Invoked when mscd_open is called. EP OUT buffer CANNOT be set to NULL!!
void tud_msc_ep_buf_cb(uint8_t **ep_buf_pp, uint32_t *ep_buf_len_p)
{
    LOGD("%s invoked!\n", __func__);
    if (ep_buf_pp != NULL)      *ep_buf_pp = my_mscd_buf;
    if (ep_buf_len_p != NULL)   *ep_buf_len_p = CFG_TUD_MSC_EP_BUFSIZE;
}

#endif // MSC_USER_EP_BUF

// Invoked to determine max LUN
uint8_t tud_msc_get_maxlun_cb(void)
{
  return 1;
  //return 2; // dual LUN
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void) lun; // use same ID for both LUNs

  const char vid[] = "LISTENAI";
  const char pid[] = "MSC_VDISK";
  const char rev[] = "1.0";

  memcpy(vendor_id , vid, strlen(vid));
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void) lun;

  return true; // RAM disk is always ready
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  *block_count = lun ? DISK1_BLOCK_NUM : DISK0_BLOCK_NUM;
  *block_size  = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void) lun;
  (void) power_condition;

  if ( load_eject )
  {
    if (start)
    {
      // load disk storage
    }else
    {
      // unload disk storage
    }
  }

  return true;
}

#if MSC_USER_BUF_DIRECT_READ
// param[out]  buffer_pp     *buffer_pp return the address of buffer which application provides to hold data to transfer (from storage to host).
// return Number of bytes actually read from storage if > 0
int32_t tud_msc_read10_cb2 (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t** buffer_pp, uint32_t bufsize)
{
    uint8_t* addr = (lun ? msc_disk1[lba] : msc_disk0_base + lba * DISK_BLOCK_SIZE) + offset;
    *buffer_pp = addr;
    return bufsize;
}

#else // !MSC_USER_BUF_DIRECT_READ
// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    uint8_t const* addr = (lun ? msc_disk1[lba] : msc_disk0_base + lba * DISK_BLOCK_SIZE) + offset;
    memcpy(buffer, addr, bufsize);
    return bufsize;
}

/*
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  //FIXME: following lines are FOR TEST ONLY!!!
  uint8_t const* addr;
  int32_t avail_size = 0;
  //LOGD("read10, lun = %d, lba = 0x%x, offset = %d, bufsize = %d\n", lun, lba, offset, bufsize);
  if (lun == 0) {
      if (lba >= DISK0_HEAD_BLOCK_NUM) {
          LOGD("read10, out of DISK0 HEAD (lba=0x%x)!\n", lba);
          memset(buffer, 0xFF, bufsize);
          return bufsize;
      } else {
          addr = msc_disk0[lba] + offset;
          //avail_size = (DISK0_HEAD_BLOCK_NUM - 1 - lba) * DISK_BLOCK_SIZE + DISK_BLOCK_SIZE - offset;
          avail_size = (DISK0_HEAD_BLOCK_NUM - lba) * DISK_BLOCK_SIZE - offset;
          TU_ASSERT (avail_size > 0);
          if (avail_size >= bufsize) {
              memcpy(buffer, addr, bufsize);
          } else {
              memcpy(buffer, addr, avail_size);
              memset(buffer + avail_size, 0xFF, bufsize - avail_size);
              LOGD("read10, partly out of DISK0 HEAD (lba=0x%x)!\n", lba);
          }
          return bufsize;
      }
  } else { // lun == 1
      if (lba >= DISK1_HEAD_BLOCK_NUM) {
          //LOGD("read10, out of DISK0 HEAD (lba=0x%x)!\n", lba);
          memset(buffer, 0xFF, bufsize);
          return bufsize;
      } else {
          addr = msc_disk1[lba] + offset;
          //avail_size = (DISK1_HEAD_BLOCK_NUM - 1 - lba) * DISK_BLOCK_SIZE + DISK_BLOCK_SIZE - offset;
          avail_size = (DISK1_HEAD_BLOCK_NUM - lba) * DISK_BLOCK_SIZE - offset;
          TU_ASSERT (avail_size > 0);
          if (avail_size >= bufsize) {
              memcpy(buffer, addr, bufsize);
          } else {
              memcpy(buffer, addr, avail_size);
              memset(buffer + avail_size, 0xFF, bufsize - avail_size);
              //LOGD("read10, partly out of DISK0 HEAD (lba=0x%x)!\n", lba);
          }
          return bufsize;
      }
  }

  return 0;
}
*/
#endif // MSC_USER_BUF_DIRECT_READ


#if MSC_USER_BUF_DIRECT_WRITE
// param[out]  buffer_pp     *buffer_pp return the address of buffer which application provides to hold data to transfer (from host to storage).
// return How many bytes that provided buffer can hold  if > 0.
int32_t tud_msc_pre_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t** buffer_pp, uint32_t bufsize)
{
    uint8_t* addr = (lun ? msc_disk1[lba] : msc_disk0_base + lba * DISK_BLOCK_SIZE) + offset;
    *buffer_pp = addr;
    return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    // DO NOTHING here now that tud_msc_pre_write10_cb is implemented to provide user buffer!
    return bufsize;
}

#else // !MSC_USER_BUF_DIRECT_WRITE
// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    uint8_t* addr = (lun ? msc_disk1[lba] : msc_disk0_base + lba * DISK_BLOCK_SIZE)  + offset;
    memcpy(addr, buffer, bufsize);
    return bufsize;
}

/*
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    //FIXME: following lines are FOR TEST ONLY!!!
    uint8_t * addr;
    int32_t avail_size = 0;
    //LOGD("write10, lun = %d, lba = 0x%x, offset = %d, bufsize = %d\n", lun, lba, offset, bufsize);
    if (lun == 0) {
        if (lba >= DISK0_HEAD_BLOCK_NUM) {
            LOGD("write10, out of DISK1 HEAD (lba=0x%x)!\n", lba);

            //TODO:
            //memset(buffer, 0, bufsize);
            return bufsize;

        } else {
            addr = msc_disk0[lba] + offset;
            //avail_size = (DISK0_HEAD_BLOCK_NUM - 1 - lba) * DISK_BLOCK_SIZE + DISK_BLOCK_SIZE - offset;
            avail_size = (DISK0_HEAD_BLOCK_NUM - lba) * DISK_BLOCK_SIZE - offset;
            TU_ASSERT (avail_size > 0);
            if (avail_size >= bufsize) {
                memcpy(addr, buffer, bufsize);
            } else {
                memcpy(addr, buffer, avail_size);

                //TODO:
                //memset(buffer + avail_size, 0, bufsize - avail_size);

                LOGD("write10, partly out of DISK1 HEAD (lba=0x%x)!\n", lba);
            }
            return bufsize;
        }

    } else { // lun == 1
        if (lba >= DISK1_HEAD_BLOCK_NUM) {
            //LOGD("write10, out of DISK1 HEAD (lba=0x%x)!\n", lba);

            //TODO:
            //memset(buffer, 0, bufsize);
            return bufsize;

        } else {
            addr = msc_disk1[lba] + offset;
            //avail_size = (DISK1_HEAD_BLOCK_NUM - 1 - lba) * DISK_BLOCK_SIZE + DISK_BLOCK_SIZE - offset;
            avail_size = (DISK1_HEAD_BLOCK_NUM - lba) * DISK_BLOCK_SIZE - offset;
            TU_ASSERT (avail_size > 0);
            if (avail_size >= bufsize) {
                memcpy(addr, buffer, bufsize);
            } else {
                memcpy(addr, buffer, avail_size);

                //TODO:
                //memset(buffer + avail_size, 0, bufsize - avail_size);
                //LOGD("write10, partly out of DISK1 HEAD (lba=0x%x)!\n", lba);
            }
            return bufsize;
        }
    }

  return 0;
}
*/
#endif // MSC_USER_BUF_DIRECT_WRITE


// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  uint16_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Host is about to read/write etc ... better not to disconnect disk
      resplen = 0;
    break;

    case SCSI_CMD_START_STOP_UNIT:
      // Host try to eject/safe remove/poweroff us. We could safely disconnect with disk storage, or go into lower power
      /*
       scsi_start_stop_unit_t const * start_stop = (scsi_start_stop_unit_t const *) scsi_cmd;
        // Start bit = 0 : low power mode, if load_eject = 1 : unmount disk storage as well
        // Start bit = 1 : Ready mode, if load_eject = 1 : mount disk storage
        start_stop->start;
        start_stop->load_eject;
       */
       resplen = 0;
    break;


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
      memcpy(buffer, response, resplen);
    }else
    {
      // SCSI output
    }
  }

  return resplen;
}

#endif
