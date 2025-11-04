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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_VENDOR)

#include "vendor_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

#if !CFG_TUD_VENDOR_USE_FIFO

typedef struct {
    uint8_t *req_buf;       /* data buffer address requested by caller */
    uint32_t req_len;        /* data total length requested by caller */
    uint32_t xfer_len;       /* already transferred data total length for the request */
} ep_xfer_info;

typedef struct {
  uint8_t ep_addr;
  uint8_t ep_type; // see tusb_xfer_type_t. 0=CTRL, 1=ISO, 2=BULK, 3=INT
  uint16_t ep_mps; // EP max. packet size
  //uint16_t ep_buf_len; // EP buffer length
  uint32_t ep_buf_len; // EP buffer length
  uint8_t *ep_buf; // Pointer to EP buffer
  ep_xfer_info ep_xfi;
} ven_ep_t;

#endif // !CFG_TUD_VENDOR_USE_FIFO


typedef struct
{
  uint8_t itf_num;

#if CFG_TUD_VENDOR_USE_FIFO

  uint8_t ep_in;
  uint8_t ep_out;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;

  uint8_t rx_ff_buf[CFG_TUD_VENDOR_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUD_VENDOR_TX_BUFSIZE];

#if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex;
  osal_mutex_def_t tx_ff_mutex;
#endif

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_VENDOR_EPSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_VENDOR_EPSIZE];

#else // !CFG_TUD_VENDOR_USE_FIFO

  ven_ep_t in_ep_array[MAX_IN_EP_COUNT];
  ven_ep_t out_ep_array[MAX_OUT_EP_COUNT];
  uint8_t in_ep_cnt;
  uint8_t out_ep_cnt;

#endif

} vendord_interface_t;

CFG_TUSB_MEM_SECTION static vendord_interface_t _vendord_itf[CFG_TUD_VENDOR];

#if CFG_TUD_VENDOR_USE_FIFO

#define ITF_MEM_RESET_SIZE   offsetof(vendord_interface_t, rx_ff)

bool tud_vendor_n_mounted (uint8_t itf)
{
  return _vendord_itf[itf].ep_in && _vendord_itf[itf].ep_out;
}

uint32_t tud_vendor_n_available (uint8_t itf)
{
  return tu_fifo_count(&_vendord_itf[itf].rx_ff);
}

bool tud_vendor_n_peek(uint8_t itf, int pos, uint8_t* u8)
{
  return tu_fifo_peek_at(&_vendord_itf[itf].rx_ff, pos, u8);
}

#else // !CFG_TUD_VENDOR_USE_FIFO

bool tud_vendor_n_mounted (uint8_t itf)
{
  return _vendord_itf[itf].in_ep_cnt || _vendord_itf[itf].out_ep_cnt;
}

#endif

//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+

#if CFG_TUD_VENDOR_USE_FIFO

static void _prep_out_transaction (vendord_interface_t* p_itf)
{
  // skip if previous transfer not complete
  if ( usbd_edpt_busy(TUD_OPT_RHPORT, p_itf->ep_out) ) return;

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  uint16_t max_read = tu_fifo_remaining(&p_itf->rx_ff);
  if ( max_read >= CFG_TUD_VENDOR_EPSIZE )
  {
    usbd_edpt_xfer(TUD_OPT_RHPORT, p_itf->ep_out, p_itf->epout_buf, CFG_TUD_VENDOR_EPSIZE);
  }
}

uint32_t tud_vendor_n_read (uint8_t itf, void* buffer, uint32_t bufsize)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  uint32_t num_read = tu_fifo_read_n(&p_itf->rx_ff, buffer, bufsize);
  _prep_out_transaction(p_itf);
  return num_read;
}

#else // !CFG_TUD_VENDOR_USE_FIFO

// Read asynchronously received bytes from OUT EP, return value:
// 0: indicate that read operation is pending, later asynchronous callback
//      will be called to notify caller of the read result (completion or failure)
// > 0: indicate that part of or all read has been done,
//      all done if return_value == bufsize, and
//      part done and asynchronous callback invoked if return_value < bufsize
// < 0: indicate that read operation failed
//
int32_t tud_vendor_n_read_immed (uint8_t itf, uint8_t ep_addr, void* buffer, uint32_t bufsize)
{
    if (itf >= CFG_TUD_VENDOR || buffer == NULL || bufsize == 0)
        return -1;

    uint8_t i;
    uint8_t const rhport = TUD_OPT_RHPORT;
    vendord_interface_t *p_vendor = &_vendord_itf[itf];
    ven_ep_t *ven_ep_p = NULL;

    for (i=0; i<MAX_OUT_EP_COUNT; i++) {
        ven_ep_p = &p_vendor->out_ep_array[i];
        if (ven_ep_p->ep_addr == ep_addr)
            break;
    }
    if (i == MAX_OUT_EP_COUNT)
        return -1;

    // claim endpoint
    if(!usbd_edpt_claim(rhport, ven_ep_p->ep_addr)) {
        TU_LOG1("%s: Failed to claim VENDOR out EP: 0x%x\r\n", __func__, ven_ep_p->ep_addr);
        return -1;
    }

    // record user buffer info.
    ven_ep_p->ep_xfi.req_buf = (uint8_t*)buffer;
    ven_ep_p->ep_xfi.req_len = bufsize;
    ven_ep_p->ep_xfi.xfer_len = 0;

    // trigger read operation for ep_out
#if DIRECT_RW_EP
    if(!usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_xfi.req_buf, ven_ep_p->ep_xfi.req_len))
#else
    if(!usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_buf, ven_ep_p->ep_buf_len))
#endif
        return -1;

    // return the length of already transferred data (usually 0, return bufsize if all is done)
    return ven_ep_p->ep_xfi.xfer_len;
}

#endif // CFG_TUD_VENDOR_USE_FIFO

//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+

#if CFG_TUD_VENDOR_USE_FIFO

static bool maybe_transmit(vendord_interface_t* p_itf)
{
  // skip if previous transfer not complete
  TU_VERIFY( !usbd_edpt_busy(TUD_OPT_RHPORT, p_itf->ep_in) );

  uint16_t count = tu_fifo_read_n(&p_itf->tx_ff, p_itf->epin_buf, CFG_TUD_VENDOR_EPSIZE);
  if (count > 0)
  {
    TU_ASSERT( usbd_edpt_xfer(TUD_OPT_RHPORT, p_itf->ep_in, p_itf->epin_buf, count) );
  }
  return true;
}

uint32_t tud_vendor_n_write (uint8_t itf, void const* buffer, uint32_t bufsize)
{
  vendord_interface_t* p_itf = &_vendord_itf[itf];
  uint16_t ret = tu_fifo_write_n(&p_itf->tx_ff, buffer, bufsize);
  maybe_transmit(p_itf);
  return ret;
}

uint32_t tud_vendor_n_write_available (uint8_t itf)
{
  return tu_fifo_remaining(&_vendord_itf[itf].tx_ff);
}

#else // !CFG_TUD_VENDOR_USE_FIFO

// Write asynchronously bytes to IN EP, return value:
// 0: indicate that write operation is pending, later asynchronous callback
//      will be called to notify caller of the write result (completion or failure)
// > 0: indicate that part of or all write has been done,
//      all done if return_value == bufsize, and
//      part done and asynchronous callback invoked if return_value < bufsize
// < 0: indicate that write operation failed//
//
int32_t tud_vendor_n_write_immed (uint8_t itf, uint8_t ep_addr, void const* buffer, uint32_t bufsize)
{
    if (itf >= CFG_TUD_VENDOR || (buffer == NULL && bufsize > 0))
        return -1;

    uint8_t i;
    uint8_t const rhport = TUD_OPT_RHPORT;
    vendord_interface_t *p_vendor = &_vendord_itf[itf];
    ven_ep_t *ven_ep_p = NULL;

    for (i=0; i<MAX_IN_EP_COUNT; i++) {
        ven_ep_p = &p_vendor->in_ep_array[i];
        if (ven_ep_p->ep_addr == ep_addr)
            break;
    }
    if (i == MAX_IN_EP_COUNT)
        return -1;

    // claim endpoint
    if(!usbd_edpt_claim(rhport, ven_ep_p->ep_addr)) {
        TU_LOG1("%s: Failed to claim VENDOR in EP: 0x%x\r\n", __func__, ven_ep_p->ep_addr);
        return -1;
    }

    // record user buffer info.
    ven_ep_p->ep_xfi.req_buf = (uint8_t*)buffer;
    ven_ep_p->ep_xfi.req_len = bufsize;
    ven_ep_p->ep_xfi.xfer_len = 0;

    uint32_t bytes_to_write = ven_ep_p->ep_xfi.req_len;

#if DIRECT_RW_EP
    // trigger write operation for ep_in
    if(!usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_xfi.req_buf, bytes_to_write))
        return -1;
#else
    if (bytes_to_write > ven_ep_p->ep_mps)
        bytes_to_write = ven_ep_p->ep_mps;
    memcpy(ven_ep_p->ep_buf, ven_ep_p->ep_xfi.req_buf, bytes_to_write);

    // trigger write operation for ep_in
    if(!usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_buf, bytes_to_write))
        return -1;
#endif

    // return the length of already transferred data (usually 0, return bufsize if all is done)
    return ven_ep_p->ep_xfi.xfer_len;
}

#endif // CFG_TUD_VENDOR_USE_FIFO

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+

#if !CFG_TUD_VENDOR_USE_FIFO

// Parse consecutive endpoint descriptors (m IN EP & n OUT EP)
bool usbd_open_edpt_array(uint8_t rhport, uint8_t const* p_desc, uint8_t itf)
{
  ven_ep_t *ven_ep_p;
  vendord_interface_t* p_vendor = &_vendord_itf[itf];

  TU_ASSERT(p_desc != NULL && itf < CFG_TUD_VENDOR);

  while (1) {
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    if (TUSB_DESC_ENDPOINT != desc_ep->bDescriptorType || desc_ep->bmAttributes.xfer > TUSB_XFER_INTERRUPT)
        break;

    TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

    if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN ) {
        TU_ASSERT(p_vendor->in_ep_cnt < MAX_IN_EP_COUNT);
        ven_ep_p = &p_vendor->in_ep_array[p_vendor->in_ep_cnt++];
    } else {
        TU_ASSERT(p_vendor->out_ep_cnt < MAX_OUT_EP_COUNT);
        ven_ep_p = &p_vendor->out_ep_array[p_vendor->out_ep_cnt++];
    }
    ven_ep_p->ep_addr = desc_ep->bEndpointAddress;
    ven_ep_p->ep_type = desc_ep->bmAttributes.xfer;
    ven_ep_p->ep_mps = desc_ep->wMaxPacketSize.size; //FIXME: SHOULD check High Speed case!

#if DIRECT_RW_EP
    ven_ep_p->ep_buf = NULL;
    ven_ep_p->ep_buf_len = 0;
#else
    tud_vendor_ep_buf_cb(itf, ven_ep_p->ep_addr, &ven_ep_p->ep_buf, &ven_ep_p->ep_buf_len);
    if (ven_ep_p->ep_buf == NULL || ven_ep_p->ep_buf_len == 0) {
        TU_LOG1("%s: NO EP buffer for EP_Addr 0x%x!\r\n", __func__, ven_ep_p->ep_addr);
        return false;
    }
#endif

    p_desc = tu_desc_next(p_desc);
  }

  // return true if at least 1 IN/OUT EP is resolved from the descriptor
  //return (p_vendor->in_ep_cnt > 0 || p_vendor->out_ep_cnt > 0);
  return true;
}

#endif // CFG_TUD_VENDOR_USE_FIFO

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void vendord_init(void)
{
  tu_memclr(_vendord_itf, sizeof(_vendord_itf));

#if CFG_TUD_VENDOR_USE_FIFO
  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++)
  {
    vendord_interface_t* p_itf = &_vendord_itf[i];

    // config fifo
    tu_fifo_config(&p_itf->rx_ff, p_itf->rx_ff_buf, CFG_TUD_VENDOR_RX_BUFSIZE, 1, false);
    tu_fifo_config(&p_itf->tx_ff, p_itf->tx_ff_buf, CFG_TUD_VENDOR_TX_BUFSIZE, 1, false);

#if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&p_itf->rx_ff, osal_mutex_create(&p_itf->rx_ff_mutex));
    tu_fifo_config_mutex(&p_itf->tx_ff, osal_mutex_create(&p_itf->tx_ff_mutex));
#endif
  }
#endif // CFG_TUD_VENDOR_USE_FIFO
}

void vendord_reset(uint8_t rhport)
{
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_VENDOR; i++)
  {
    vendord_interface_t* p_itf = &_vendord_itf[i];

#if CFG_TUD_VENDOR_USE_FIFO
    tu_memclr(p_itf, ITF_MEM_RESET_SIZE);
    tu_fifo_clear(&p_itf->rx_ff);
    tu_fifo_clear(&p_itf->tx_ff);
#else // !CFG_TUD_VENDOR_USE_FIFO
    tu_memclr(p_itf, sizeof(vendord_interface_t));
#endif
  }
}

uint16_t vendord_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass, 0);

  uint16_t const drv_len = sizeof(tusb_desc_interface_t) + itf_desc->bNumEndpoints*sizeof(tusb_desc_endpoint_t);
  TU_VERIFY(max_len >= drv_len, 0);

  // Find available interface
  vendord_interface_t* p_vendor = NULL;
  uint8_t i = 0;
  for(; i<CFG_TUD_VENDOR; i++)
  {
#if CFG_TUD_VENDOR_USE_FIFO
    if ( _vendord_itf[i].ep_in == 0 && _vendord_itf[i].ep_out == 0 )
#else // !CFG_TUD_VENDOR_USE_FIFO
    if ( _vendord_itf[i].in_ep_cnt == 0 && _vendord_itf[i].out_ep_cnt == 0 )
#endif
    {
      p_vendor = &_vendord_itf[i];
      break;
    }
  }
  TU_VERIFY(p_vendor, 0);

#if CFG_TUD_VENDOR_USE_FIFO
  // Open endpoint pair with usbd helper
  TU_ASSERT(usbd_open_edpt_pair(rhport, tu_desc_next(itf_desc), 2, TUSB_XFER_BULK, &p_vendor->ep_out, &p_vendor->ep_in), 0);
#else // !CFG_TUD_VENDOR_USE_FIFO
  // Open endpoint array with usbd helper
  TU_ASSERT(usbd_open_edpt_array(rhport, tu_desc_next(itf_desc), i), 0);

#endif

  p_vendor->itf_num = itf_desc->bInterfaceNumber;

  //BSD: NO user buffer, NO RX started!
#if CFG_TUD_VENDOR_USE_FIFO
  // Prepare for incoming data
  if ( !usbd_edpt_xfer(rhport, p_vendor->ep_out, p_vendor->epout_buf, sizeof(p_vendor->epout_buf)) )
  {
    TU_LOG1_FAILED();
    TU_BREAKPOINT();
  }
#endif //CFG_TUD_VENDOR_USE_FIFO

  return drv_len;
}

#if CFG_TUD_VENDOR_USE_FIFO

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) result;

  uint8_t itf = 0;
  vendord_interface_t* p_itf = _vendord_itf;

  for ( ; ; itf++, p_itf++)
  {
    if (itf >= TU_ARRAY_SIZE(_vendord_itf)) return false;

    if ( ( ep_addr == p_itf->ep_out ) || ( ep_addr == p_itf->ep_in ) ) break;
  }

  if ( ep_addr == p_itf->ep_out )
  {
    // Receive new data
    tu_fifo_write_n(&p_itf->rx_ff, p_itf->epout_buf, xferred_bytes);

    // Invoked callback if any
    if (tud_vendor_rx_cb) tud_vendor_rx_cb(itf);

    _prep_out_transaction(p_itf);
  }
  else if ( ep_addr == p_itf->ep_in )
  {
    // Send complete, try to send more if possible
    maybe_transmit(p_itf);
  }

  return true;
}

#else // !CFG_TUD_VENDOR_USE_FIFO

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) result;

  uint8_t itf, i, is_out, max_count;
  vendord_interface_t* p_itf;
  ven_ep_t *ven_ep_p = NULL;

  if (ep_addr & 0x80) { // IN EP
      is_out = 0;
      max_count = MAX_IN_EP_COUNT;
  } else { // OUT EP
      is_out = 1;
      max_count = MAX_OUT_EP_COUNT;
  }

  for (itf = 0, p_itf = _vendord_itf; itf < CFG_TUD_VENDOR; itf++, p_itf++) {
    ven_ep_p = (is_out ? &p_itf->out_ep_array[0] : &p_itf->in_ep_array[0]);
    for (i=0; i<max_count; i++, ven_ep_p++) {
        if (ven_ep_p->ep_addr == ep_addr)
            break;
    }
    if (i < max_count)
        break;
  }

  if (itf == CFG_TUD_VENDOR || ep_addr != ven_ep_p->ep_addr) {
      TU_LOG1("EP(addr=0x%x) does NOT belong to Vendor-defined class\r\n");
      return false;
  }

  if ( is_out ) { // OUT EP
    if (xferred_bytes > 0) {
    #if !DIRECT_RW_EP
      memcpy(ven_ep_p->ep_xfi.req_buf + ven_ep_p->ep_xfi.xfer_len, ven_ep_p->ep_buf, xferred_bytes);
    #endif
      ven_ep_p->ep_xfi.xfer_len += xferred_bytes;
    }

#if DIRECT_RW_EP
    if (tud_vendor_read_done_cb)
        tud_vendor_read_done_cb(itf, ven_ep_p->ep_addr, VENDOR_OP_STATUS_SUCCESS,
                                ven_ep_p->ep_xfi.req_buf, ven_ep_p->ep_xfi.xfer_len);
#else
    // Invoked callback if any
    if (tud_vendor_rx_cb) tud_vendor_rx_cb(itf);

    // invoke last done callback or continue reading
    if (ven_ep_p->ep_xfi.xfer_len == ven_ep_p->ep_xfi.req_len || xferred_bytes < ven_ep_p->ep_mps) {
        if (tud_vendor_read_done_cb)
            tud_vendor_read_done_cb(itf, ven_ep_p->ep_addr, VENDOR_OP_STATUS_SUCCESS,
                                    ven_ep_p->ep_xfi.req_buf, ven_ep_p->ep_xfi.xfer_len);
    } else {
        // trigger read operation for ep_out
        if(usbd_edpt_claim(rhport, ven_ep_p->ep_addr)) {
            usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_buf, ven_ep_p->ep_buf_len);
        }
    }
#endif // DIRECT_RW_EP

  } else { // IN EP
      ven_ep_p->ep_xfi.xfer_len += xferred_bytes;
      uint32_t bytes_to_write = ven_ep_p->ep_xfi.req_len - ven_ep_p->ep_xfi.xfer_len;

    #if DIRECT_RW_EP
      if (bytes_to_write > 0) {
          if ( usbd_edpt_claim(rhport, ven_ep_p->ep_addr) )
              usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_xfi.req_buf+ven_ep_p->ep_xfi.xfer_len, bytes_to_write);
      }
    #else
      if (bytes_to_write > ven_ep_p->ep_mps)
          bytes_to_write = ven_ep_p->ep_mps;
      if (bytes_to_write > 0) {
          memcpy(ven_ep_p->ep_buf, ven_ep_p->ep_xfi.req_buf+ven_ep_p->ep_xfi.xfer_len, bytes_to_write);
          if ( usbd_edpt_claim(rhport, ven_ep_p->ep_addr) )
              usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, ven_ep_p->ep_buf, bytes_to_write);
      }
    #endif

      //TODO: Send ZLP if bytes_to_write is changed to 0 for first time?
      // If there is no data left, a ZLP CAN be sent if xferred_bytes (NOT 0)
      // is multiple of EP Packet size...
      //
      // NEW CHANGE: DON'T send ZLP automatically, and upper layer can send ZLP
      // with tud_vendor_write_zlp() by itself if necessary...

//#if AUTO_TX_ZLP
//      if (xferred_bytes > 0 && bytes_to_write == 0 &&
//          (xferred_bytes % ven_ep_p->ep_mps) == 0) {
//        if ( usbd_edpt_claim(rhport, ven_ep_p->ep_addr) ) {
//            usbd_edpt_xfer(rhport, ven_ep_p->ep_addr, NULL, 0);
//            TU_LOG1("%s: Send ZLP\r\n", __func__);
//        }
//      } else
//#endif // AUTO_TX_ZLP

      if (ven_ep_p->ep_xfi.xfer_len == ven_ep_p->ep_xfi.req_len) {
          // invoke last done callback
          if (tud_vendor_write_done_cb )
              tud_vendor_write_done_cb(itf, ven_ep_p->ep_addr, VENDOR_OP_STATUS_SUCCESS,
                                  ven_ep_p->ep_xfi.req_buf, ven_ep_p->ep_xfi.xfer_len);
      }

  } // IN EP

  return true;
}

#endif // CFG_TUD_VENDOR_USE_FIFO


//--------------------------------------------------------------------+
// Callbacks (except those Weak callbacks)
//--------------------------------------------------------------------+
#define WARN_CB_NOT_IMPL()    \
    TU_LOG1("Callback \'%s\' SHOULD be implemented!\r\n", __FUNCTION__)

#if !CFG_TUD_VENDOR_USE_FIFO

//BSD: set EP buffer and its length, Invoked when vendord_open is called. EP buffer CANNOT be set to NULL!!
TU_ATTR_WEAK void tud_vendor_ep_buf_cb(uint8_t itf, uint8_t ep_addr, uint8_t **ep_buf_pp, uint32_t *ep_buf_len_p)
{
    (void) itf; (void) ep_addr;
    if (ep_buf_pp != NULL)      *ep_buf_pp = NULL;
    if (ep_buf_len_p != NULL)   *ep_buf_len_p = 0;
    WARN_CB_NOT_IMPL();
}

#endif // !CFG_TUD_VENDOR_USE_FIFO

#endif
