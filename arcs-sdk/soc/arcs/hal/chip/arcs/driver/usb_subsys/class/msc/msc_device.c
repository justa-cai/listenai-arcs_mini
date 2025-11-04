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
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_MSC)

#include "common/tusb_common.h"
#include "msc_device.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"         // for faking dcd_event_xfer_complete

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
enum
{
  MSC_STAGE_CMD  = 0,
  MSC_STAGE_DATA,
  MSC_STAGE_STATUS,
  MSC_STAGE_STATUS_SENT
};

typedef struct
{
  // To optimize alignment change the order of fields: 31 + 1 + 13 + 3 + 4*N
  CFG_TUSB_MEM_ALIGN msc_cbw_t cbw;
  uint8_t  itf_num;

  CFG_TUSB_MEM_ALIGN msc_csw_t csw;

//  uint8_t  itf_num;
  uint8_t  ep_in;
  uint8_t  ep_out;

  // Bulk Only Transfer (BOT) Protocol
  uint8_t  stage;
  uint32_t total_len;
  uint32_t xferred_len; // numbered of bytes transferred so far in the Data Stage
  uint32_t to_xfer_len; // numbered of bytes to be transferred in the last xxx_xfer_cb (BSD:used in write only)

  // Sense Response Data
  uint8_t sense_key;
  uint8_t add_sense_code;
  uint8_t add_sense_qualifier;
  uint8_t rsvd1;

  uint8_t *ep_buf; // Pointer to EP buffer, mainly used for EP OUT (occasionally for EP IN)
  uint32_t ep_buf_len; // EP buffer length

  uint8_t *ep_in_buf; // TMP Pointer to EP IN data buffer (may equal to ep_buf, or may be changed with user TX data)
  //uint32_t ep_in_len; // EP IN buffer data length

  uint8_t *ep_out_buf; // TMP Pointer to EP OUT data buffer (may equal to ep_buf, or may be changed with user RX data)
  //uint32_t ep_out_len; // EP OUT buffer data length

}mscd_interface_t;

//BSD NOTE: only 1 MSC device instance is supported according to original design!!
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static mscd_interface_t _mscd_itf;

#if !MSC_USER_EP_BUF
//BSD NOTE: only 1 MSC device instance is supported according to original design
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t _mscd_buf[CFG_TUD_MSC_EP_BUFSIZE];
#endif

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static int32_t proc_builtin_scsi(uint8_t lun, uint8_t const scsi_cmd[16], uint8_t* buffer, uint32_t bufsize);
static void proc_read10_cmd(uint8_t rhport, mscd_interface_t* p_msc);
static void proc_write10_cmd(uint8_t rhport, mscd_interface_t* p_msc);

static inline uint32_t rdwr10_get_lba(uint8_t const command[])
{
/*
  // read10 & write10 has the same format
  scsi_write10_t* p_rdwr10 = (scsi_write10_t*) command;

  // copy first to prevent mis-aligned access
  uint32_t lba;
  // use offsetof to avoid pointer to the odd/misaligned address
  memcpy(&lba, (uint8_t*) p_rdwr10 + offsetof(scsi_write10_t, lba), 4);

  // lba is in Big Endian format
  return tu_ntohl(lba);
*/
    //BSD changed:
    uint16_t idx = offsetof(scsi_write10_t, lba);
    return (command[idx] << 24) | (command[idx+1] << 16) | (command[idx+2] << 8) | command[idx+3];

}

static inline uint16_t rdwr10_get_blockcount(uint8_t const command[])
{
/*
  // read10 & write10 has the same format
  scsi_write10_t* p_rdwr10 = (scsi_write10_t*) command;

  // copy first to prevent mis-aligned access
  uint16_t block_count;
  // use offsetof to avoid pointer to the odd/misaligned address
  memcpy(&block_count, (uint8_t*) p_rdwr10 + offsetof(scsi_write10_t, block_count), 2);
  return tu_ntohs(block_count);
*/
    //BSD changed:
    uint16_t idx = offsetof(scsi_write10_t, block_count);
    return (command[idx] << 8) | command[idx+1] ;
}

//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= 2

static tu_lookup_entry_t const _msc_scsi_cmd_lookup[] =
{
  { .key = SCSI_CMD_TEST_UNIT_READY              , .data = "Test Unit Ready" },
  { .key = SCSI_CMD_INQUIRY                      , .data = "Inquiry" },
  { .key = SCSI_CMD_MODE_SELECT_6                , .data = "Mode_Select 6" },
  { .key = SCSI_CMD_MODE_SENSE_6                 , .data = "Mode_Sense 6" },
  { .key = SCSI_CMD_START_STOP_UNIT              , .data = "Start Stop Unit" },
  { .key = SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL , .data = "Prevent Allow Medium Removal" },
  { .key = SCSI_CMD_READ_CAPACITY_10             , .data = "Read Capacity10" },
  { .key = SCSI_CMD_REQUEST_SENSE                , .data = "Request Sense" },
  { .key = SCSI_CMD_READ_FORMAT_CAPACITY         , .data = "Read Format Capacity" },
  { .key = SCSI_CMD_READ_10                      , .data = "Read10" },
  { .key = SCSI_CMD_WRITE_10                     , .data = "Write10" }
};

static tu_lookup_table_t const _msc_scsi_cmd_table =
{
  .count = TU_ARRAY_SIZE(_msc_scsi_cmd_lookup),
  .items = _msc_scsi_cmd_lookup
};

#endif

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code, uint8_t add_sense_qualifier)
{
  (void) lun;

  _mscd_itf.sense_key           = sense_key;
  _mscd_itf.add_sense_code      = add_sense_code;
  _mscd_itf.add_sense_qualifier = add_sense_qualifier;

  return true;
}


// In some case tud_msc_read10_cb return 0 to indicate data is not ready yet and the process is pending.
// Users can call tud_msc_notify_read_ready() to notify tinyUSB that data is ready and the process can go on...
void tud_msc_notify_read_ready(uint8_t rhport)
{
    mscd_interface_t* p_msc = &_mscd_itf;

    // zero means not ready -> simulate an transfer complete so that this driver callback will fired again
    dcd_event_xfer_complete(rhport, p_msc->ep_in, 0, XFER_RESULT_SUCCESS, true);
}

// In some case tud_msc_write10_cb return 0 to indicate data process is pending.
// User can call tud_msc_notify_write_done() to notify tinyUSB that data write operation is done...
void tud_msc_notify_write_done(uint8_t rhport)
{
    mscd_interface_t* p_msc = &_mscd_itf;

    // simulate an transfer complete with adjusted parameters --> this driver callback will fired again
    dcd_event_xfer_complete(rhport, p_msc->ep_out, p_msc->to_xfer_len, XFER_RESULT_SUCCESS, true);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void mscd_init(void)
{
  tu_memclr(&_mscd_itf, sizeof(mscd_interface_t));
}

void mscd_reset(uint8_t rhport)
{
  (void) rhport;
  tu_memclr(&_mscd_itf, sizeof(mscd_interface_t));
  TU_LOG1(" %s: \n", __func__);
}

uint16_t mscd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
    TU_LOG1(" %s: bInterfaceNumber = %d\n", __func__, itf_desc->bInterfaceNumber);
  // only support SCSI's BOT protocol
  TU_VERIFY(TUSB_CLASS_MSC    == itf_desc->bInterfaceClass &&
            MSC_SUBCLASS_SCSI == itf_desc->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == itf_desc->bInterfaceProtocol, 0);

  // msc driver length is fixed
  uint16_t const drv_len = sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  // Max length mus be at least 1 interface + 2 endpoints
  TU_ASSERT(max_len >= drv_len, 0);

  mscd_interface_t * p_msc = &_mscd_itf;
  p_msc->itf_num = itf_desc->bInterfaceNumber;

  // Open endpoint pair
  TU_ASSERT( usbd_open_edpt_pair(rhport, tu_desc_next(itf_desc), 2, TUSB_XFER_BULK, &p_msc->ep_out, &p_msc->ep_in), 0 );

#if MSC_USER_EP_BUF
  // Get RX buffer of EP OUT via callback
  tud_msc_ep_buf_cb( &p_msc->ep_buf, &p_msc->ep_buf_len);
  if (p_msc->ep_buf == NULL || p_msc->ep_buf_len == 0) {
      TU_LOG1("%s: NO RX buffer for EP OUT!\n", __func__);
      return 0;
  }
#else
  p_msc->ep_buf = &_mscd_buf[0];
  p_msc->ep_buf_len = CFG_TUD_MSC_EP_BUFSIZE;
#endif

  // Prepare for Command Block Wrapper
  if ( !usbd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) )
  {
    TU_LOG1_FAILED();
    TU_BREAKPOINT();
  }

  return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool mscd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * p_request)
{
  // nothing to do with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  // Handle class request only
  TU_VERIFY(p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  switch ( p_request->bRequest )
  {
    case MSC_REQ_RESET:
      // TODO: Actually reset interface.
      tud_control_status(rhport, p_request);
    break;

    case MSC_REQ_GET_MAX_LUN:
    {
      uint8_t maxlun = 1;
      if (tud_msc_get_maxlun_cb) maxlun = tud_msc_get_maxlun_cb();
      TU_VERIFY(maxlun);

      // MAX LUN is minus 1 by specs
      maxlun--;

      tud_control_xfer(rhport, p_request, &maxlun, 1);
    }
    break;

    default:
        TU_LOG1("MSC Req = 0x%x, Type = 0x%x, Value = 0x%x, Index = 0x%x, Length = %d\r\n",
                p_request->bRequest, p_request->bmRequestType,
                p_request->wValue, p_request->wIndex, p_request->wLength);
        return false; // stall unsupported request
  }

  return true;
}

bool mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  mscd_interface_t* p_msc = &_mscd_itf;
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  msc_csw_t       * p_csw = &p_msc->csw;

  switch (p_msc->stage)
  {
    case MSC_STAGE_CMD:
      //------------- new CBW received -------------//
      // Complete IN while waiting for CMD is usually Status of previous SCSI op, ignore it
      if(ep_addr != p_msc->ep_out) return true;

      TU_ASSERT( event == XFER_RESULT_SUCCESS &&
                 xferred_bytes == sizeof(msc_cbw_t) && p_cbw->signature == MSC_CBW_SIGNATURE );

      TU_LOG2("  SCSI Command: %s\r\n", tu_lookup_find(&_msc_scsi_cmd_table, p_cbw->command[0]));
      // TU_LOG2_MEM(p_cbw, xferred_bytes, 2);

      p_csw->signature    = MSC_CSW_SIGNATURE;
      p_csw->tag          = p_cbw->tag;
      p_csw->data_residue = 0;

      /*------------- Parse command and prepare DATA -------------*/
      p_msc->stage = MSC_STAGE_DATA;
      p_msc->total_len = p_cbw->total_bytes;
      p_msc->xferred_len = 0;
      p_msc->to_xfer_len = 0; //BSD:

      if (SCSI_CMD_READ_10 == p_cbw->command[0])
      {
        proc_read10_cmd(rhport, p_msc);
      }
      else if (SCSI_CMD_WRITE_10 == p_cbw->command[0])
      {
        proc_write10_cmd(rhport, p_msc);
      }
      else
      {
        // For other SCSI commands
        // 1. OUT : queue transfer (invoke app callback after done)
        // 2. IN & Zero: Process if is built-in, else Invoke app callback. Skip DATA if zero length
        if ( (p_cbw->total_bytes > 0 ) && !tu_bit_test(p_cbw->dir, 7) )
        {
          // queue transfer
          //TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_out, _mscd_buf, p_msc->total_len) );
          TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_out, p_msc->ep_buf, p_msc->total_len) );
        }else
        {
          int32_t resplen;

          // First process if it is a built-in commands
          //resplen = proc_builtin_scsi(p_cbw->lun, p_cbw->command, _mscd_buf, sizeof(_mscd_buf));
          resplen = proc_builtin_scsi(p_cbw->lun, p_cbw->command, p_msc->ep_buf, p_msc->ep_buf_len);

          // Not built-in, invoke user callback
          if ( (resplen < 0) && (p_msc->sense_key == 0) )
          {
            //resplen = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, _mscd_buf, p_msc->total_len);
            resplen = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, p_msc->ep_buf, p_msc->total_len);
          }

          if ( resplen < 0 )
          {
            p_msc->total_len = 0;
            p_csw->status = MSC_CSW_STATUS_FAILED;
            p_msc->stage = MSC_STAGE_STATUS;

            // failed but senskey is not set: default to Illegal Request
            if ( p_msc->sense_key == 0 ) tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

            // Stall bulk In if needed
            if (p_cbw->total_bytes) usbd_edpt_stall(rhport, p_msc->ep_in);
            TU_LOG1("stall EP 0x%x for cmd = 0x%x\n", p_msc->ep_in, p_cbw->command[0]);
          }
          else
          {
            p_msc->total_len = (uint32_t) resplen;
            p_csw->status = MSC_CSW_STATUS_PASSED;

            if (p_msc->total_len)
            {
              TU_ASSERT( p_cbw->total_bytes >= p_msc->total_len ); // cannot return more than host expect
              //TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_in, _mscd_buf, p_msc->total_len) );
              TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_in, p_msc->ep_buf, p_msc->total_len) );
            }else
            {
              p_msc->stage = MSC_STAGE_STATUS;
            }
          }
        }
      }
    break;

    case MSC_STAGE_DATA:
      TU_LOG2("  SCSI Data\r\n");
      //TU_LOG2_MEM(_mscd_buf, xferred_bytes, 2);

      // OUT transfer, invoke callback if needed
      if ( !tu_bit_test(p_cbw->dir, 7) )
      {
        if ( SCSI_CMD_WRITE_10 != p_cbw->command[0] )
        {
          //int32_t cb_result = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, _mscd_buf, p_msc->total_len);
          int32_t cb_result = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, p_msc->ep_buf, p_msc->total_len);

          if ( cb_result < 0 )
          {
            p_csw->status = MSC_CSW_STATUS_FAILED;
            tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
          }else
          {
            p_csw->status = MSC_CSW_STATUS_PASSED;
          }
        }
        else
        {
          uint16_t const block_sz = p_cbw->total_bytes / rdwr10_get_blockcount(p_cbw->command);

          // Adjust lba with transferred bytes
          uint32_t const lba = rdwr10_get_lba(p_cbw->command) + (p_msc->xferred_len / block_sz);

          // Application can consume smaller bytes
          //int32_t nbytes = tud_msc_write10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, _mscd_buf, xferred_bytes);
          int32_t nbytes = tud_msc_write10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, p_msc->ep_out_buf, xferred_bytes);

          if ( nbytes < 0 )
          {
            // negative means error -> skip to status phase, status in CSW set to failed
            p_csw->data_residue = p_cbw->total_bytes - p_msc->xferred_len;
            p_csw->status       = MSC_CSW_STATUS_FAILED;
            p_msc->stage        = MSC_STAGE_STATUS;

            tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
            break;
          }else
          {
            // Application consume less than what we got (including zero)
            if ( nbytes < (int32_t) xferred_bytes )
            {
              if ( nbytes > 0 )
              {
                p_msc->xferred_len += nbytes;
                //memmove(_mscd_buf, _mscd_buf+nbytes, xferred_bytes-nbytes);
                memmove(p_msc->ep_out_buf, p_msc->ep_out_buf+nbytes, xferred_bytes-nbytes);
              }
              p_msc->to_xfer_len = xferred_bytes - nbytes;

              //BSD: DON'T CALL dcd_event_xfer_complete here, it could cause endless recursive loop!
              // dcd_event_xfer_complete SHOULD be called in another execution path, e.g. another ISR or thread context.
              //
              //// simulate an transfer complete with adjusted parameters --> this driver callback will fired again
              //dcd_event_xfer_complete(rhport, p_msc->ep_out, xferred_bytes-nbytes, XFER_RESULT_SUCCESS, false);

              return true; // skip the rest
            }
            else
            {
              // Application consume all bytes in our buffer. Nothing to do, process with normal flow
            }
          }
        }
      }

      // Accumulate data so far
      p_msc->xferred_len += xferred_bytes;

      if ( p_msc->xferred_len >= p_msc->total_len )
      {
        // Data Stage is complete
        p_msc->stage = MSC_STAGE_STATUS;
      }
      else
      {
        // READ10 & WRITE10 Can be executed with large bulk of data e.g write 8K bytes (several flash write)
        // We break it into multiple smaller command whose data size is up to CFG_TUD_MSC_EP_BUFSIZE
        if (SCSI_CMD_READ_10 == p_cbw->command[0])
        {
          proc_read10_cmd(rhport, p_msc);
        }
        else if (SCSI_CMD_WRITE_10 == p_cbw->command[0])
        {
          proc_write10_cmd(rhport, p_msc);
        }else
        {
          // No other command take more than one transfer yet -> unlikely error
          TU_BREAKPOINT();
          TU_LOG1("DATA STAGE ERROR: cmd = %d!!!\n", p_cbw->command[0]);
        }
      }
    break;

    case MSC_STAGE_STATUS:
      // processed immediately after this switch, supposedly to be empty
    break;

    case MSC_STAGE_STATUS_SENT:
      // Wait for the Status phase to complete
      if( (ep_addr == p_msc->ep_in) && (xferred_bytes == sizeof(msc_csw_t)) )
      {
        TU_LOG2("  SCSI Status: %u\r\n", p_csw->status);
        // TU_LOG2_MEM(p_csw, xferred_bytes, 2);

        // Invoke complete callback if defined
        // Note: There is racing issue with samd51 + qspi flash testing with arduino
        // if complete_cb() is invoked after queuing the status.
        switch(p_cbw->command[0])
        {
          case SCSI_CMD_READ_10:
            if ( tud_msc_read10_complete_cb ) tud_msc_read10_complete_cb(p_cbw->lun);
          break;

          case SCSI_CMD_WRITE_10:
            if ( tud_msc_write10_complete_cb ) tud_msc_write10_complete_cb(p_cbw->lun);
          break;

          default:
            if ( tud_msc_scsi_complete_cb ) tud_msc_scsi_complete_cb(p_cbw->lun, p_cbw->command);
          break;
        }

        // Move to default CMD stage
        p_msc->stage = MSC_STAGE_CMD;

        // Queue for the next CBW
        TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) );
      }
    break;

    default : break;
  }

  if ( p_msc->stage == MSC_STAGE_STATUS )
  {
    // Either endpoints is stalled, need to wait until it is cleared by host
    if ( usbd_edpt_stalled(rhport,  p_msc->ep_in) || usbd_edpt_stalled(rhport,  p_msc->ep_out) )
    {
      //FIXME:
      //BSD: DON'T CALL dcd_event_xfer_complete here, it could cause endless recursive loop!
      // dcd_event_xfer_complete SHOULD be called in another execution path, e.g. another ISR or thread context.

/*
      // simulate an transfer complete with adjusted parameters --> this driver callback will fired again
      // and response with status phase after halted endpoints are cleared.
      // note: use ep_out to prevent confusing with STATUS complete
      dcd_event_xfer_complete(rhport, p_msc->ep_out, 0, XFER_RESULT_SUCCESS, false);
*/
    }
    else
    {
      // Move to Status Sent stage
      p_msc->stage = MSC_STAGE_STATUS_SENT;

      // Send SCSI Status
      TU_ASSERT(usbd_edpt_xfer(rhport, p_msc->ep_in , (uint8_t*) &p_msc->csw, sizeof(msc_csw_t)));
    }
  }

  return true;
}

/*------------------------------------------------------------------*/
/* SCSI Command Process
 *------------------------------------------------------------------*/

// return response's length (copied to buffer). Negative if it is not an built-in command or indicate Failed status (CSW)
// In case of a failed status, sense key must be set for reason of failure
static int32_t proc_builtin_scsi(uint8_t lun, uint8_t const scsi_cmd[16], uint8_t* buffer, uint32_t bufsize)
{
  (void) bufsize; // TODO refractor later
  int32_t resplen;

  switch ( scsi_cmd[0] )
  {
    case SCSI_CMD_TEST_UNIT_READY:
      resplen = 0;
      if ( !tud_msc_test_unit_ready_cb(lun) )
      {
        // Failed status response
        resplen = - 1;

        // If sense key is not set by callback, default to Logical Unit Not Ready, Cause Not Reportable
        if ( _mscd_itf.sense_key == 0 ) tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x04, 0x00);
      }
    break;

    case SCSI_CMD_START_STOP_UNIT:
      resplen = 0;

      if (tud_msc_start_stop_cb)
      {
        scsi_start_stop_unit_t const * start_stop = (scsi_start_stop_unit_t const *) scsi_cmd;
        if ( !tud_msc_start_stop_cb(lun, start_stop->power_condition, start_stop->start, start_stop->load_eject) )
        {
          // Failed status response
          resplen = - 1;

          // If sense key is not set by callback, default to Logical Unit Not Ready, Cause Not Reportable
          if ( _mscd_itf.sense_key == 0 ) tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x04, 0x00);
        }
      }
    break;

    case SCSI_CMD_READ_CAPACITY_10:
    {
      uint32_t block_count;
      uint32_t block_size;
      uint16_t block_size_u16;

      tud_msc_capacity_cb(lun, &block_count, &block_size_u16);
      block_size = (uint32_t) block_size_u16;

      // Invalid block size/count from callback, possibly unit is not ready
      // stall this request, set sense key to NOT READY
      if (block_count == 0 || block_size == 0)
      {
        resplen = -1;

        // If sense key is not set by callback, default to Logical Unit Not Ready, Cause Not Reportable
        if ( _mscd_itf.sense_key == 0 ) tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x04, 0x00);
      }else
      {
        scsi_read_capacity10_resp_t read_capa10;

        read_capa10.last_lba = tu_htonl(block_count-1);
        read_capa10.block_size = tu_htonl(block_size);

        resplen = sizeof(read_capa10);
        memcpy(buffer, &read_capa10, resplen);
      }
    }
    break;

    case SCSI_CMD_READ_FORMAT_CAPACITY:
    {
      scsi_read_format_capacity_data_t read_fmt_capa =
      {
          .list_length     = 8,
          .block_num       = 0,
          .descriptor_type = 2, // formatted media
          .block_size_u16  = 0
      };

      uint32_t block_count;
      uint16_t block_size;

      tud_msc_capacity_cb(lun, &block_count, &block_size);

      // Invalid block size/count from callback, possibly unit is not ready
      // stall this request, set sense key to NOT READY
      if (block_count == 0 || block_size == 0)
      {
        resplen = -1;

        // If sense key is not set by callback, default to Logical Unit Not Ready, Cause Not Reportable
        if ( _mscd_itf.sense_key == 0 ) tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x04, 0x00);
      }else
      {
        read_fmt_capa.block_num = tu_htonl(block_count);
        read_fmt_capa.block_size_u16 = tu_htons(block_size);

        resplen = sizeof(read_fmt_capa);
        memcpy(buffer, &read_fmt_capa, resplen);
      }
    }
    break;

    case SCSI_CMD_INQUIRY:
    {
      scsi_inquiry_resp_t inquiry_rsp =
      {
          .is_removable         = 1,
          .version              = 2,
          .response_data_format = 2,
      };

      // vendor_id, product_id, product_rev is space padded string
      memset(inquiry_rsp.vendor_id  , ' ', sizeof(inquiry_rsp.vendor_id));
      memset(inquiry_rsp.product_id , ' ', sizeof(inquiry_rsp.product_id));
      memset(inquiry_rsp.product_rev, ' ', sizeof(inquiry_rsp.product_rev));

      tud_msc_inquiry_cb(lun, inquiry_rsp.vendor_id, inquiry_rsp.product_id, inquiry_rsp.product_rev);

      resplen = sizeof(inquiry_rsp);
      memcpy(buffer, &inquiry_rsp, resplen);
    }
    break;

    case SCSI_CMD_MODE_SENSE_6:
    {
      scsi_mode_sense6_resp_t mode_resp =
      {
          .data_len = 3,
          .medium_type = 0,
          .write_protected = false,
          .reserved = 0,
          .block_descriptor_len = 0  // no block descriptor are included
      };

      bool writable = true;
      if (tud_msc_is_writable_cb) {
          writable = tud_msc_is_writable_cb(lun);
      }
      mode_resp.write_protected = !writable;

      resplen = sizeof(mode_resp);
      memcpy(buffer, &mode_resp, resplen);
    }
    break;

    case SCSI_CMD_REQUEST_SENSE:
    {
      scsi_sense_fixed_resp_t sense_rsp =
      {
          .response_code = 0x70,
          .valid         = 1
      };

      sense_rsp.add_sense_len = sizeof(scsi_sense_fixed_resp_t) - 8;

      sense_rsp.sense_key           = _mscd_itf.sense_key;
      sense_rsp.add_sense_code      = _mscd_itf.add_sense_code;
      sense_rsp.add_sense_qualifier = _mscd_itf.add_sense_qualifier;

      resplen = sizeof(sense_rsp);
      memcpy(buffer, &sense_rsp, resplen);

      // Clear sense data after copy
      tud_msc_set_sense(lun, 0, 0, 0);
    }
    break;

    default: resplen = -1; break;
  }

  return resplen;
}

static void proc_read10_cmd(uint8_t rhport, mscd_interface_t* p_msc)
{
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  msc_csw_t       * p_csw = &p_msc->csw;

  uint16_t const block_cnt = rdwr10_get_blockcount(p_cbw->command);
  TU_ASSERT(block_cnt, ); // prevent div by zero

  uint16_t const block_sz = p_cbw->total_bytes / block_cnt;
  TU_ASSERT(block_sz, ); // prevent div by zero

  // Adjust lba with transferred bytes
  uint32_t const lba = rdwr10_get_lba(p_cbw->command) + (p_msc->xferred_len / block_sz);

  int32_t nbytes;

#if  MSC_USER_BUF_DIRECT_READ

  // let application decide to consume how many bytes //JUST HERE!! BSD20250530.
  nbytes = tud_msc_read10_cb2(p_cbw->lun, lba, p_msc->xferred_len % block_sz, &p_msc->ep_in_buf, p_cbw->total_bytes-p_msc->xferred_len);
  //p_msc->ep_in_len = nbytes;

#else // !MSC_USER_BUF_DIRECT_READ
  // remaining bytes capped at class buffer
  //int32_t nbytes = (int32_t) tu_min32(sizeof(_mscd_buf), p_cbw->total_bytes-p_msc->xferred_len);
  nbytes = (int32_t) tu_min32(p_msc->ep_buf_len, p_cbw->total_bytes-p_msc->xferred_len);

  // Application can consume smaller bytes
  //nbytes = tud_msc_read10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, _mscd_buf, (uint32_t) nbytes);
  nbytes = tud_msc_read10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, p_msc->ep_buf, (uint32_t) nbytes);
  p_msc->ep_in_buf = p_msc->ep_buf;
  //p_msc->ep_in_len = nbytes;

#endif // MSC_USER_BUF_DIRECT_READ

  if ( nbytes < 0 )
  {
    // negative means error -> pipe is stalled & status in CSW set to failed
    p_csw->data_residue = p_cbw->total_bytes - p_msc->xferred_len;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Sense = Invalid Command Operation
    usbd_edpt_stall(rhport, p_msc->ep_in);
  }
  else if ( nbytes == 0 )
  {
    //BSD: DON'T CALL dcd_event_xfer_complete here, it could cause endless recursive loop!
    // dcd_event_xfer_complete SHOULD be called in another execution path, e.g. another ISR or thread context.
    //
    //// zero means not ready -> simulate an transfer complete so that this driver callback will fired again
    //dcd_event_xfer_complete(rhport, p_msc->ep_in, 0, XFER_RESULT_SUCCESS, false);
  }
  else
  {
    //TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_in, _mscd_buf, nbytes), );
    TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_in, p_msc->ep_in_buf, nbytes), );
  }
}

static void proc_write10_cmd(uint8_t rhport, mscd_interface_t* p_msc)
{
  msc_cbw_t const * p_cbw = &p_msc->cbw;
  bool writable = true;
  if (tud_msc_is_writable_cb) {
    writable = tud_msc_is_writable_cb(p_cbw->lun);
  }
  if (!writable) {
    msc_csw_t* p_csw = &p_msc->csw;
    p_csw->data_residue = p_cbw->total_bytes;
    p_csw->status       = MSC_CSW_STATUS_FAILED;

    tud_msc_set_sense(p_cbw->lun, SCSI_SENSE_DATA_PROTECT, 0x27, 0x00); // Sense = Write protected
    usbd_edpt_stall(rhport, p_msc->ep_out);
    return;
  }

  int32_t nbytes;

#if  MSC_USER_BUF_DIRECT_WRITE
  uint16_t const block_sz = p_cbw->total_bytes / rdwr10_get_blockcount(p_cbw->command);

  // Adjust lba with transferred bytes
  uint32_t const lba = rdwr10_get_lba(p_cbw->command) + (p_msc->xferred_len / block_sz);

  // let application decide to consume how many bytes //JUST HERE!! BSD20250530.
  nbytes = tud_msc_pre_write10_cb(p_cbw->lun, lba, p_msc->xferred_len % block_sz, &p_msc->ep_out_buf, p_cbw->total_bytes-p_msc->xferred_len);
  //p_msc->ep_out_len = nbytes;
  if (p_msc->ep_out_buf == NULL) {
      p_msc->ep_out_buf = p_msc->ep_buf;
      //p_msc->ep_out_len = p_msc->ep_buf_len;
      nbytes = p_msc->ep_buf_len;
  }
#else // !MSC_USER_BUF_DIRECT_WRITE
  // remaining bytes capped at class buffer
  //int32_t nbytes = (int32_t) tu_min32(sizeof(_mscd_buf), p_cbw->total_bytes-p_msc->xferred_len);
  nbytes = (int32_t) tu_min32(p_msc->ep_buf_len, p_cbw->total_bytes-p_msc->xferred_len);
  p_msc->ep_out_buf = p_msc->ep_buf;
  //p_msc->ep_out_len = p_msc->ep_buf_len;
#endif // MSC_USER_BUF_DIRECT_WRITE

  // Write10 callback will be called later when usb transfer complete
  //TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_out, _mscd_buf, nbytes), );
  TU_ASSERT( usbd_edpt_xfer(rhport, p_msc->ep_out, p_msc->ep_out_buf, nbytes), );
}


//--------------------------------------------------------------------+
// Callbacks (except those Weak callbacks)
//--------------------------------------------------------------------+
#define WARN_CB_NOT_IMPL()    \
    TU_LOG1("Callback \'%s\' SHOULD be implemented!\r\n", __FUNCTION__)

#if MSC_USER_EP_BUF
//BSD: set EP OUT buffer and its length, Invoked when mscd_open is called. EP OUT buffer CANNOT be set to NULL!!
TU_ATTR_WEAK void tud_msc_ep_buf_cb(uint8_t **ep_buf_pp, uint32_t *ep_buf_len_p)
{
    if (ep_buf_pp != NULL)      *ep_buf_pp = NULL;
    if (ep_buf_len_p != NULL)   *ep_buf_len_p = 0;
    WARN_CB_NOT_IMPL();
}
#endif // MSC_USER_EP_BUF

/**
 * Invoked when received \ref SCSI_CMD_READ_10 command
 */
TU_ATTR_WEAK int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    (void) lun; (void) lba; (void) offset; (void) buffer; (void) bufsize;
    WARN_CB_NOT_IMPL();
    return -1;
}

/**
 * Invoked when received \ref SCSI_CMD_WRITE_10 command
 */
TU_ATTR_WEAK int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    (void) lun; (void) lba; (void) offset; (void) buffer; (void) bufsize;
    WARN_CB_NOT_IMPL();
    return -1;
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
TU_ATTR_WEAK void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void) lun; (void) vendor_id; (void) product_id; (void) product_rev;
    WARN_CB_NOT_IMPL();
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
TU_ATTR_WEAK bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void) lun;
    WARN_CB_NOT_IMPL();
    return false;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
TU_ATTR_WEAK void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    (void) lun; (void) block_count; (void) block_size;
    WARN_CB_NOT_IMPL();
}

/**
 * Invoked when received an SCSI command not in built-in list below.
 */
TU_ATTR_WEAK int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    WARN_CB_NOT_IMPL();
    return -1;
}

#endif
