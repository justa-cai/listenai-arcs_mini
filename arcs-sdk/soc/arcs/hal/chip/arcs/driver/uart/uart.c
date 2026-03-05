/*
 * Copyright (c) 2013-2020 CSK Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdint.h>
#include "arcs_ap.h"
#include "dma.h"
#include "uart_reg.h"
#include "uart.h"
#include "PowerManager.h"
#include "ClockManager.h"
#if CONFIG_PM && PM_UART_WAKEUP
#include "pm_impl.h"
#endif

#define CSK_UART_DRV_VERSION    CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* driver version */

/* Driver Version */
static const CSK_DRIVER_VERSION DriverVersion = {
     CSK_UART_API_VERSION,
     CSK_UART_DRV_VERSION
 };

#define CHECK_RESOURCES(res)  do{\
        if((res != &uart0_resources) && (res != &uart1_resources) && (res != &uart2_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

#if CONFIG_PM && PM_UART_WAKEUP
#define UART_PM_STATE_IDLE         0
#define UART_PM_STATE_BUSY         1

extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);

static int32_t UART_check_idle(pm_mode_t mode);

static pm_peripheral_dev_t uart_pm_dev =
{
    .name = "uart",
    .pm_suspend = NULL,
    .pm_resume  = NULL,
    .pm_check_idle = UART_check_idle
};
struct uart_pm_info_t
{
    int32_t state;
    uint64_t active_time;
} uart_pm_info;
#endif
/*
 * UART0 DEVICE
 * */
static void UART0_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void UART0_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void UART0_IRQ_Handler(void);

static UART_DMA uart0_tx_dma_info = {
        CSK_UART0_TX_DMA_CH,
        CSK_UART0_TX_DMA_REQSEL,
        UART0_DMA_TX_Handler,
};

static UART_DMA uart0_rx_dma_info = {
        CSK_UART0_RX_DMA_CH,
        CSK_UART0_RX_DMA_REQSEL,
        UART0_DMA_RX_Handler,
};

static UART_INFO uart0_info = { 0 };

static UART_RESOURCES uart0_resources = {
	IP_UART0,// REG
	IRQ_UART0_VECTOR,// IRQ NUM
    UART0_IRQ_Handler,// IRQ HANDLER
	UART_TX_FIFO_SIZE,
    &uart0_tx_dma_info,// TX DMA INFO
    &uart0_rx_dma_info,// RX DMA INFO
    &uart0_info,// TRANS INFO
    (CSK_UART_AUTO_FLOW_EN_RTS | CSK_UART_AUTO_FLOW_EN_CTS),
};

/*
 * UART1 DEVICE
 * */

static void UART1_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void UART1_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void UART1_IRQ_Handler(void);

static UART_DMA uart1_tx_dma_info = {
        CSK_UART1_TX_DMA_CH,
		CSK_UART1_TX_DMA_REQSEL,
        UART1_DMA_TX_Handler,
};

static UART_DMA uart1_rx_dma_info = {
        CSK_UART1_RX_DMA_CH,
		CSK_UART1_RX_DMA_REQSEL,
        UART1_DMA_RX_Handler,
};

static UART_INFO uart1_info = { 0 };

static UART_RESOURCES uart1_resources = {
	IP_UART1,// REG
	IRQ_UART1_VECTOR,// IRQ NUM
    UART1_IRQ_Handler,// IRQ HANDLER
	UART_TX_FIFO_SIZE,
    &uart1_tx_dma_info,// TX DMA INFO
    &uart1_rx_dma_info,// RX DMA INFO
    &uart1_info,// TRANS INFO
    (CSK_UART_AUTO_FLOW_EN_RTS | CSK_UART_AUTO_FLOW_EN_CTS),
};

/*
 * UART2 DEVICE
 * */
static void UART2_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void UART2_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void UART2_IRQ_Handler(void);

static UART_DMA uart2_tx_dma_info = {
		CSK_UART2_TX_DMA_CH,
		CSK_UART2_TX_DMA_REQSEL,
        UART2_DMA_TX_Handler,
};

static UART_DMA uart2_rx_dma_info = {
		CSK_UART2_RX_DMA_CH,
		CSK_UART2_RX_DMA_REQSEL,
        UART2_DMA_RX_Handler,
};

static UART_INFO uart2_info = { 0 };

static UART_RESOURCES uart2_resources = {
	IP_UART2,// REG
	IRQ_UART2_VECTOR,// IRQ NUM
    UART2_IRQ_Handler,// IRQ HANDLER
	UART_TX_FIFO_SIZE,
    &uart2_tx_dma_info,// TX DMA INFO
    &uart2_rx_dma_info,// RX DMA INFO
    &uart2_info,// TRANS INFO
    (CSK_UART_AUTO_FLOW_EN_RTS | CSK_UART_AUTO_FLOW_EN_CTS),
};



/*
 * DEVICE DESCRIPTION FUNCTION
 * */

void* UART0(void){
    return (void*)&uart0_resources;
}

void* UART1(void){
    return (void*)&uart1_resources;
}

void* UART2(void){
    return (void*)&uart2_resources;
}

#define MAX_VALUE_M                    1024UL
#define MAX_VALUE_N                    1024UL

//greatest common divisor
static uint32_t compute_gcd(uint32_t a, uint32_t c)
{
	uint32_t t;
    while(c != 0) {
    	t = a % c;
        a = c;
        c = t;
    }
    return a;
}

static uint32_t compute_n(uint32_t n1, uint32_t m1, uint32_t m)
{
	uint32_t n;
	int32_t err0 = 0, err1 = 1 << 30;

	n = (uint64_t)m * n1 / m1;
	if(n) {
		for(; n < m; n++) {
			err0 = (uint64_t)m * n1 - (uint64_t)m1 * n;
			if(err0 <= 0) break;
			else err1 = err0;
		}
		// compare err0 and err1
		if(err0) {
			err0 = -err0;   // change to positive number
			n = err0 <= err1 ? n : (n - 1);
		}
	}
	return n;
}
static int32_t __uart_compute_div(void* res, uint32_t baudrate, uint32_t *pm, uint32_t *pn, uint32_t div)
{
	uint32_t gcd, m, n, tmp;
	int32_t ret = 0;

	tmp = div * baudrate;
	uint32_t uart_clk;

    if((uint32_t)UART0() == (uint32_t)res){
    	uart_clk = CRM_GetUart0Freq();
    } else if ((uint32_t)UART1() == (uint32_t)res){
    	uart_clk = CRM_GetUart1Freq();
    } else {
        uart_clk = CRM_GetUart2Freq();
    }

	gcd = compute_gcd(uart_clk, tmp);
	m = uart_clk / gcd;
	n = tmp / gcd;

	do {
		if(n * 2 > m) {  // n/m must less equal than 1/2
			ret = -1;
			goto error_ret;
		}
		if(m > MAX_VALUE_M) {
			n = compute_n(n, m, MAX_VALUE_N-1);
			if(n) {
				*pm = MAX_VALUE_M-1;
				*pn = n;
			} else {
				ret = -1;
				goto error_ret;
			}
		} else {
			*pm = m ; //- 1;
			*pn = n;
		}
	} while(0);

	ret = (int64_t)tmp - (int64_t)((uint64_t)*pn * (uint64_t)uart_clk / (uint64_t)*pm);
    if (ret < 0){
        ret = -ret;
    }

	error_ret:

	return ret;
}
int32_t uart_compute_div(void* res, uint32_t baudrate, uint32_t *pm, uint32_t *pn, uint32_t *pdiv)
{
	int32_t ret = 0;
	uint32_t m_4, n_4, m_16, n_16;
	int32_t err = 0, err1 = 0;

	do {
        err = __uart_compute_div(res, baudrate, &m_4, &n_4, 4);

        err1 = __uart_compute_div(res, baudrate, &m_16, &n_16, 16);

        if (err == -1 && err1 == -1){
            ret = -1;
            break;
        } else {
            err = ((uint32_t)err & 0x7FFFFFFF);
            err1 = ((uint32_t)err1 & 0x7FFFFFFF);
        }

        // Choose div 16
        if (err > err1){
            *pm = m_16;
            *pn = n_16;
            *pdiv = 16;
        } else { // Choose div 4
            *pm = m_4;
            *pn = n_4;
            *pdiv = 4;
        }
	} while(0);

	return ret;
}

//
//   Functions
//
CSK_DRIVER_VERSION UART_GetVersion(void)
{
   return DriverVersion;
}

int32_t UART_Initialize(void *res, CSK_UART_SignalEvent_t cb_event, void* workspace)
{

    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if (uart->info->flags & UART_FLAG_INITIALIZED) {
        // Driver is already initialized
        return CSK_DRIVER_OK;
    }

    // Initialize UART Run-time Resources
    uart->info->cb_event = cb_event;

    uart->info->workspace = workspace;

    uart->info->rx_status.rx_busy = 0U;
    uart->info->rx_status.rx_overflow = 0U;
    uart->info->rx_status.rx_break = 0U;
    uart->info->rx_status.rx_framing_error = 0U;
    uart->info->rx_status.rx_parity_error = 0U;

    uart->info->xfer.send_active = 0U;
    uart->info->xfer.tx_def_val = 0U;
    uart->info->xfer.rx_num = 0U;
    uart->info->xfer.tx_num = 0U;
    uart->info->xfer.rx_buf = NULL;
    uart->info->xfer.tx_buf = NULL;
    uart->info->xfer.rx_cnt = 0U;
    uart->info->xfer.tx_cnt = 0U;

    uart->info->tx_trig_lvl = UART_TX_TRIG_LVL;
    uart->info->rx_trig_lvl = UART_RX_TRIG_LVL;
    uart->info->baudrate = 0U;
    uart->info->mode = 0U;

    uart->info->timeout = 0U;
    uart->info->inter_en = 0U;

    uart->info->flags = UART_FLAG_INITIALIZED;
#if CONFIG_PM && PM_UART_WAKEUP
    pm_peripheral_register(&uart_pm_dev);
#endif

    return CSK_DRIVER_OK;
}

int32_t UART_Uninitialize(void *res)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if (uart->info->flags == 0) {
        // Driver is already uninitialized
        return CSK_DRIVER_OK;
    }

    // DMA Uninitialize
    if (!uart->info->inter_en) {
        dma_uninitialize();
    }

    // Reset UART status flags
    uart->info->cb_event = NULL;

    uart->info->workspace = NULL;

    uart->info->rx_status.rx_busy = 0U;
    uart->info->rx_status.rx_overflow = 0U;
    uart->info->rx_status.rx_break = 0U;
    uart->info->rx_status.rx_framing_error = 0U;
    uart->info->rx_status.rx_parity_error = 0U;

    uart->info->xfer.send_active = 0U;
    uart->info->xfer.tx_def_val = 0U;
    uart->info->xfer.rx_num = 0U;
    uart->info->xfer.tx_num = 0U;
    uart->info->xfer.rx_buf = NULL;
    uart->info->xfer.tx_buf = NULL;
    uart->info->xfer.rx_cnt = 0U;
    uart->info->xfer.tx_cnt = 0U;

    uart->info->tx_trig_lvl = 0;
    uart->info->rx_trig_lvl = 0;
    uart->info->baudrate = 0U;
    uart->info->mode = 0U;
    uart->info->flags = 0U;

    uart->info->timeout = 0U;
    uart->info->inter_en = 0U;

    return CSK_DRIVER_OK;
}

int32_t UART_PowerControl(void *res, CSK_POWER_STATE state)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    switch (state)
    {
    case CSK_POWER_OFF:
        uart->reg->REG_CTRL.all = 0;
        // Disable UART IRQ
        disable_IRQ(uart->irq_num);
        // Clear driver variables
        uart->info->rx_status.rx_busy = 0U;
        uart->info->rx_status.rx_overflow = 0U;
        uart->info->rx_status.rx_break = 0U;
        uart->info->rx_status.rx_framing_error = 0U;
        uart->info->rx_status.rx_parity_error = 0U;
        uart->info->xfer.send_active = 0U;
        uart->info->flags &= ~UART_FLAG_POWERED;
        // Uninstall IRQ Handler
        register_ISR(uart->irq_num, NULL, NULL);

        if ((uart->info->flags & UART_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }


        if (!(uart->info->inter_en) && (uart->info->xfer.send_active != 0U)){
            dma_channel_disable(uart->dma_tx->channel, 0);
        }

        if (!(uart->info->inter_en) && (uart->info->rx_status.rx_busy)){
            dma_channel_disable(uart->dma_rx->channel, 0);
        }

        break;
    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    case CSK_POWER_FULL:
        if ((uart->info->flags & UART_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }
        if ((uart->info->flags & UART_FLAG_POWERED) != 0U) {
            return CSK_DRIVER_OK;
        }

        // Power and clock manager
        // Reset current peripheral
        if (uart == &uart0_resources){
            __HAL_CRM_UART0_CLK_ENABLE();
        	__HAL_PMU_UART0_RST_ENABLE();
        }else if (uart == &uart1_resources){
            __HAL_CRM_UART1_CLK_ENABLE();
        	__HAL_PMU_UART1_RST_ENABLE();
        }else if (uart == &uart2_resources){
            __HAL_CRM_UART2_CLK_ENABLE();
            __HAL_PMU_UART2_RST_ENABLE();
        } else {
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        // Disable interrupts
        uart->reg->REG_CTRL.all = 0;
        uart->reg->REG_IRQ_MASK.all = 0;
        uart->info->flags = UART_FLAG_POWERED | UART_FLAG_INITIALIZED;

        // Configure interrupt handle
        register_ISR(uart->irq_num, uart->irq_handler, NULL);
        enable_IRQ(uart->irq_num);

        break;
    }
    return CSK_DRIVER_OK;
}

//PTCH_DFN(void, UART_log32, uint8_t, log_id, uint32_t, log_data);
void UART_log32(uint8_t log_id, uint32_t log_data)
{
//    PTCH_FST(void, UART_log32, uint8_t, log_id, uint32_t, log_data);
    return;

#if (CONTROL_LOG_OVER_TLOG)
    (*(volatile uint32_t *)(UART1_BASE+0x8)) =5; // length for tokenizer handle message
#endif
    (*(volatile uint32_t *)(UART1_BASE+0x8)) =log_id;
    (*(volatile uint32_t *)(UART1_BASE+0x8)) =(log_data>>24);
    (*(volatile uint32_t *)(UART1_BASE+0x8)) =(log_data>>16);
    (*(volatile uint32_t *)(UART1_BASE+0x8)) =(log_data>>8);
    (*(volatile uint32_t *)(UART1_BASE+0x8)) =log_data;

    return;
}
void UART_logN(uint8_t log_id, uint8_t *log_data_ptr, uint8_t log_data_length)
{
	for(uint32_t i=0; i<log_data_length; i+=4)
	{
		(*(volatile uint32_t *)(UART1_BASE)) =log_id++;
		(*(volatile uint32_t *)(UART1_BASE)) =log_data_ptr[i];
		(*(volatile uint32_t *)(UART1_BASE)) =log_data_ptr[i+1];
		(*(volatile uint32_t *)(UART1_BASE)) =log_data_ptr[i+2];
		(*(volatile uint32_t *)(UART1_BASE)) =log_data_ptr[i+3];
		//while (!((*(volatile uint32_t *)(UART1_BASE+0X14))&UARTC_LSR_THRE));
	}

	return;
}

void UART_TLOG(uint8_t log_size, uint8_t *log_data_ptr)
{
    (*(volatile uint32_t *)(UART1_BASE)) =log_size;  // length for tokenizer process controller log
	for(uint32_t i=0; i<log_size; i++)
	{
		(*(volatile uint32_t *)(UART1_BASE)) =log_data_ptr[i];
		//while (!((*(volatile uint32_t *)(UART1_BASE+0X14))&UARTC_LSR_THRE));
	}

	return;
}



int32_t UART_Send(void *res, const void *data, uint32_t num)
{
	uint32_t val;

    CHECK_RESOURCES(res);
    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((data == NULL) || (num == 0U)) {
        // Invalid parameters
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((uart->info->flags & UART_FLAG_CONFIGURED) == 0U) {
        // UART is not configured (mode not selected)
        return CSK_DRIVER_ERROR;
    }

    if (!(uart->info->flags & UART_FLAG_DIS_TX_INT) && (uart->info->xfer.send_active != 0U)) {
        // Send is not completed yet
        return CSK_DRIVER_ERROR_BUSY;
    }

    // Set Send active flag
    uart->info->xfer.send_active = 1U;

    // Save transmit buffer info
    uart->info->xfer.tx_buf = (uint8_t *) data;
    uart->info->xfer.tx_num = num;
    uart->info->xfer.tx_cnt = 0U;

    // DMA transmit
    if (!uart->info->inter_en){
        int32_t stat;

        dma_channel_select(&uart->dma_tx->channel, uart->dma_tx->cb_event,
            0, DMA_CACHE_SYNC_SRC);
        if (uart->dma_tx->channel == DMA_CHANNEL_ANY) {
            return CSK_DRIVER_ERROR;
        }
//        uart->reg->REG_IRQ_MASK.all |= UART_TX_DMA_DONE;

        stat = dma_channel_configure (uart->dma_tx->channel,
                    (uint32_t) uart->info->xfer.tx_buf,
                    (uint32_t) (&(uart->reg->REG_RXTX_BUFFER.all )),
                    uart->info->xfer.tx_num,
                    DMA_CH_CTLL_DST_WIDTH(CSK_UART_TX_DMA_WIDTH_PARA) | DMA_CH_CTLL_SRC_WIDTH(CSK_UART_TX_DMA_WIDTH_PARA) |\
                    DMA_CH_CTLL_DST_BSIZE(CSK_UART_TX_DMA_BSIZE_PARA) | DMA_CH_CTLL_SRC_BSIZE(CSK_UART_TX_DMA_BSIZE_PARA) |\
                    DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |\
                    DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                    DMA_CH_CFGL_CH_PRIOR(1), // config_low
                    DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_DST_PER(uart->dma_tx->reqsel), // config_high
                    0, 0);

        if(stat == -1){
            return CSK_DRIVER_ERROR;
        }
    } else {
        // Fill TX FIFO
        if (!(uart->info->flags & UART_FLAG_DIS_TX_INT)) {
            // Fill TX FIFO
            val = uart->reg->REG_STATUS.bit.TX_FIFO_SPACE;
            if (val) {
                while ((val--)&& (uart->info->xfer.tx_cnt != uart->info->xfer.tx_num)) {
                    hal_SendByte(uart->reg, uart->info->xfer.tx_buf[uart->info->xfer.tx_cnt]);
                    uart->info->xfer.tx_cnt++;
                }
            }
            // Enable transmit holding register empty interrupt
            uart->reg->REG_IRQ_MASK.all |= UART_TX_DATA_NEEDED;
        }
        else {
             while ((uart->info->xfer.tx_cnt < uart->info->xfer.tx_num)) {
                while(!uart->reg->REG_STATUS.bit.TX_FIFO_SPACE);
                hal_SendByte(uart->reg, uart->info->xfer.tx_buf[uart->info->xfer.tx_cnt]);
                uart->info->xfer.tx_cnt++;
             }
        }
    }

    return CSK_DRIVER_OK;
}

int32_t UART_Send_IT(void *res, const void *data, uint32_t num)
{
	uint32_t val;

    CHECK_RESOURCES(res);
    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((data == NULL) || (num == 0U)) {
        // Invalid parameters
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((uart->info->flags & UART_FLAG_CONFIGURED) == 0U) {
        // UART is not configured (mode not selected)
        return CSK_DRIVER_ERROR;
    }

    if (!(uart->info->flags & UART_FLAG_DIS_TX_INT) && (uart->info->xfer.send_active != 0U)) {
        // Send is not completed yet
        return CSK_DRIVER_ERROR_BUSY;
    }

    // Set Send active flag
    uart->info->xfer.send_active = 1U;

    // Save transmit buffer info
    uart->info->xfer.tx_buf = (uint8_t *) data;
    uart->info->xfer.tx_num = num;
    uart->info->xfer.tx_cnt = 0U;

	// Fill TX FIFO
	val = uart->reg->REG_STATUS.bit.TX_FIFO_SPACE;
	if (val) {
		while ((val--)&& (uart->info->xfer.tx_cnt != uart->info->xfer.tx_num)) {
			hal_SendByte(uart->reg, uart->info->xfer.tx_buf[uart->info->xfer.tx_cnt]);
			uart->info->xfer.tx_cnt++;
		}
	}
	// Enable transmit holding register empty interrupt
	uart->reg->REG_IRQ_MASK.all |= UART_TX_DATA_NEEDED;

    return CSK_DRIVER_OK;
}

int32_t UART_Receive(void *res, void *data, uint32_t num)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((data == NULL) || (num == 0U)) {
        // Invalid parameters
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((uart->info->flags & UART_FLAG_CONFIGURED) == 0U) {
        // UART is not configured (mode not selected)
        return CSK_DRIVER_ERROR;
    }

    // Check if receiver is busy
    if (uart->info->rx_status.rx_busy == 1U) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    // Set RX busy flag
    uart->info->rx_status.rx_busy = 1U;

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

    // DMA transmit
    if (!uart->info->inter_en){
        int32_t stat;

        dma_channel_select(&uart->dma_rx->channel, uart->dma_rx->cb_event,
            0, DMA_CACHE_SYNC_DST);
        if (uart->dma_rx->channel == DMA_CHANNEL_ANY) {
            return CSK_DRIVER_ERROR;
        }
        stat = dma_channel_configure (uart->dma_rx->channel,
                    (uint32_t) (&(uart->reg->REG_RXTX_BUFFER.all )),
                    (uint32_t) uart->info->xfer.rx_buf,
                    uart->info->xfer.rx_num,
                    DMA_CH_CTLL_DST_WIDTH(CSK_UART_RX_DMA_WIDTH_PARA) | DMA_CH_CTLL_SRC_WIDTH(CSK_UART_RX_DMA_WIDTH_PARA) |\
                    DMA_CH_CTLL_DST_BSIZE(CSK_UART_RX_DMA_BSIZE_PARA) | DMA_CH_CTLL_SRC_BSIZE(CSK_UART_RX_DMA_BSIZE_PARA) |\
                    DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
                    DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                    DMA_CH_CFGL_CH_PRIOR(1), // config_low
                    DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(uart->dma_rx->reqsel), // config_high
                    0, 0);

        if(stat == -1){
            return CSK_DRIVER_ERROR;
        }
        if(uart->info->timeout) {
			uart->reg->REG_IRQ_MASK.all |= UART_RX_DMA_TIMEOUT;
		}

    } else{
        // Enable receive data available interrupt
    	if(uart->info->timeout) {
			uart->reg->REG_IRQ_MASK.all |= UART_RX_TIMEOUT | UART_RX_LINE_ERR | UART_RX_DATA_AVAILABLE;
		} else {
			uart->reg->REG_IRQ_MASK.all |= UART_RX_LINE_ERR | UART_RX_DATA_AVAILABLE;
		}
    }

    return CSK_DRIVER_OK;
}

int32_t UART_Receive_IT(void *res, void *data, uint32_t num)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((data == NULL) || (num == 0U)) {
        // Invalid parameters
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((uart->info->flags & UART_FLAG_CONFIGURED) == 0U) {
        // UART is not configured (mode not selected)
        return CSK_DRIVER_ERROR;
    }

    // Check if receiver is busy
    if (uart->info->rx_status.rx_busy == 1U) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    // Set RX busy flag
    uart->info->rx_status.rx_busy = 1U;

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


	// Enable receive data available interrupt
	if(uart->info->timeout) {
		uart->reg->REG_IRQ_MASK.all |= UART_RX_TIMEOUT | UART_RX_LINE_ERR | UART_RX_DATA_AVAILABLE;
	} else {
		uart->reg->REG_IRQ_MASK.all |= UART_RX_LINE_ERR | UART_RX_DATA_AVAILABLE;
	}

    return CSK_DRIVER_OK;
}


uint32_t UART_GetTxCount(void *res)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((!uart->info->inter_en) && uart->dma_tx && uart->info->xfer.send_active) {
        uart->info->xfer.tx_cnt = dma_channel_get_count(uart->dma_tx->channel);
    }

    return uart->info->xfer.tx_cnt;
}

uint32_t UART_GetRxCount(void *res)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((!uart->info->inter_en) && uart->dma_rx && uart->info->rx_status.rx_busy) {
        uart->info->xfer.rx_cnt = dma_channel_get_count(uart->dma_rx->channel);
    }

    return uart->info->xfer.rx_cnt;
}

int32_t UART_Control(void *res, uint32_t control, uint32_t arg)
{    
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    if ((uart->info->flags & UART_FLAG_POWERED) == 0U) {
        // UART not powered
        return CSK_DRIVER_ERROR;
    }

    // Set uart modem
    switch (control & CSK_UART_CONTROL_Msk) {
    	case CSK_UART_CONTROL_TX:    	 // Control TX
    	case CSK_UART_CONTROL_RX:        // Control RX
    		if (arg) {
    			if((control & CSK_UART_CONTROL_Msk) == CSK_UART_CONTROL_TX) {
    				uart->info->flags |= UART_FLAG_TX_ENABLED;
				} else {
					uart->info->flags |= UART_FLAG_RX_ENABLED;
				}
    			if(!uart->reg->REG_CTRL.bit.ENABLE) {
    				uart->reg->REG_CTRL.bit.ENABLE = 1;          // enable uart
    				uart->reg->REG_CMD_SET.bit.TX_FIFO_RESET = 1; // reset tx fifo
					uart->reg->REG_CMD_SET.bit.RX_FIFO_RESET = 1; // reset rx fifo
					uart->reg->REG_STATUS.all = 1;                    // clear line error bits
    			}
			} else {
				if((control & CSK_UART_CONTROL_Msk) == CSK_UART_CONTROL_TX) {
					uart->info->flags &= ~UART_FLAG_TX_ENABLED;
					uart->reg->REG_IRQ_MASK.all &= ~(UART_TX_MODEM_STATUS | UART_TX_DATA_NEEDED | UART_TX_DMA_DONE);
				} else {
					uart->info->flags &= ~UART_FLAG_RX_ENABLED;
					uart->reg->REG_IRQ_MASK.all &= ~(UART_RX_DATA_AVAILABLE | UART_RX_TIMEOUT
										| UART_RX_LINE_ERR | UART_RX_DMA_DONE | UART_RX_DMA_TIMEOUT);
				}

				if((uart->info->flags & (UART_FLAG_TX_ENABLED | UART_FLAG_RX_ENABLED)) == 0) {
					uart->reg->REG_CTRL.bit.ENABLE = 0;
				}
			}
			return CSK_DRIVER_OK;
    	case CSK_UART_ABORT_SEND:        // Abort Send
			// Disable transmit holding register empty interrupt
			uart->reg->REG_IRQ_MASK.all &= ~(UART_TX_MODEM_STATUS | UART_TX_DATA_NEEDED | UART_TX_DMA_DONE);
			// If DMA mode - disable DMA channel
			if ((!uart->info->inter_en) && (uart->info->xfer.send_active != 0U)) {
				dma_channel_disable(uart->dma_tx->channel, 1);
			}
			// Clear Send active flag
			uart->info->xfer.send_active = 0U;
			return CSK_DRIVER_OK;
    	case CSK_UART_ABORT_RECEIVE:        // Abort receive
			// Disable receive data available interrupt
			uart->reg->REG_IRQ_MASK.all &= ~(UART_RX_DATA_AVAILABLE | UART_RX_TIMEOUT
					| UART_RX_LINE_ERR | UART_RX_DMA_DONE | UART_RX_DMA_TIMEOUT);
			// If DMA mode - disable DMA channel
			if ((!uart->info->inter_en) && (uart->info->rx_status.rx_busy)) {
				dma_channel_disable(uart->dma_rx->channel, 1);
			}
			// Clear RX busy status
			uart->info->rx_status.rx_busy = 0U;
			return CSK_DRIVER_OK;
        case CSK_UART_DISABLE_TX_INT:
            if (arg) {
                uart->info->flags |= UART_FLAG_DIS_TX_INT;
            }
            else {
                uart->info->flags &= ~UART_FLAG_DIS_TX_INT;
            }
            return CSK_DRIVER_OK;
		default:
			break;
    }

    // dma or interrupt
    {
        switch (control & CSK_UART_Function_CONTROL_Msk) {
            case CSK_UART_Function_CONTROL_Dma:
            	uart->reg->REG_CTRL.bit.DMA_MODE = 1;
                uart->info->inter_en = 0;

                // UART2 DMA handshake select
                if(uart == &uart2_resources) {
                    IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_00 = 1;
                    IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_01 = 1;
                }
                // DMA initialize
                dma_initialize();
                uart->reg->REG_TRIGGERS.bit.RX_TRIGGER = 1;   // reserve a byte for timeout interrupt
                uart->reg->REG_TRIGGERS.bit.TX_TRIGGER = UART_TX_TRIG_LVL;
                break;
            case CSK_UART_Function_CONTROL_Int:
            	uart->reg->REG_CTRL.bit.DMA_MODE = 0;
                uart->info->inter_en = 1;
				uart->reg->REG_TRIGGERS.bit.RX_TRIGGER = 0;
				uart->reg->REG_TRIGGERS.bit.TX_TRIGGER = UART_TX_TRIG_LVL;
                break;
            default:
            	return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // Set uart mode
    {
        uint32_t mode = 0, m, n, div, uart_base, cmn_reg;
        switch (control & CSK_UART_CONTROL_Msk) {
			case CSK_UART_MODE_ASYNCHRONOUS:
				mode = CSK_UART_MODE_ASYNCHRONOUS;
				uart->info->timeout = 0;
				uart->info->half_duplex = 0;
				break;
			case CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT:
				mode = CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT;
				uart->info->timeout = 1;
				uart->info->half_duplex = 0;
				break;
			default:
				return CSK_DRIVER_ERROR_UNSUPPORTED;
        }
        // set baud rate. TODO: clk select
        if(mode) {
			if((uint32_t)UART0() == (uint32_t)res) {
				HAL_CRM_SetUart0ClkDiv(1, 1);
			} else if ((uint32_t)UART1() == (uint32_t)res){
				HAL_CRM_SetUart1ClkDiv(1, 1);
			} else {
                HAL_CRM_SetUart2ClkDiv(1, 1);
            }

			if(uart_compute_div(res, arg, &m, &n, &div) < 0) {
				return CSK_DRIVER_ERROR_UNSUPPORTED;
			}
			if(div == 16) {
				uart->reg->REG_CTRL.bit.DIVISOR_MODE = 1;  //0: sclk/4; 1: sclk/16
			} else {
				uart->reg->REG_CTRL.bit.DIVISOR_MODE = 0;
			}
			if((uint32_t)UART0() == (uint32_t)res) {
				HAL_CRM_SetUart0ClkDiv(n, m);
			}  else if ((uint32_t)UART1() == (uint32_t)res){
				HAL_CRM_SetUart1ClkDiv(n, m);
			} else {
                HAL_CRM_SetUart2ClkDiv(n, m);
            }
		}
    }

    // UART Data bits
    switch (control & CSK_UART_DATA_BITS_Msk) {
		case CSK_UART_DATA_BITS_7:
			uart->reg->REG_CTRL.bit.DATA_BITS = 0;
			break;
		case CSK_UART_DATA_BITS_8:
			uart->reg->REG_CTRL.bit.DATA_BITS = 1;
			break;
		default:
			return CSK_UART_ERROR_DATA_BITS;
    }

    // UART Parity
    switch (control & CSK_UART_PARITY_Msk) {
		case CSK_UART_PARITY_NONE:
			uart->reg->REG_CTRL.bit.PARITY_ENABLE = 0;
			break;
		case CSK_UART_PARITY_EVEN:
			uart->reg->REG_CTRL.bit.PARITY_ENABLE = 1;
			uart->reg->REG_CTRL.bit.PARITY_SELECT = 1;
			break;
		case CSK_UART_PARITY_ODD:
			uart->reg->REG_CTRL.bit.PARITY_ENABLE = 1;
			uart->reg->REG_CTRL.bit.PARITY_SELECT = 0;
			break;
		default:
			return CSK_UART_ERROR_PARITY;
    }

    // UART Stop bits
    switch (control & CSK_UART_STOP_BITS_Msk) {
		case CSK_UART_STOP_BITS_1:
			uart->reg->REG_CTRL.bit.TX_STOP_BITS = 0;
			break;
		case CSK_UART_STOP_BITS_2:
			uart->reg->REG_CTRL.bit.TX_STOP_BITS = 1;
			break;
		default:
			return CSK_UART_ERROR_STOP_BITS;
    }

    // auto flow control
    {
        switch (control & CSK_UART_FLOW_CONTROL_Msk) {
			case CSK_UART_FLOW_CONTROL_NONE:
				uart->reg->REG_CTRL.bit.AUTO_FLOW_CONTROL = 0;
				uart->reg->REG_CMD_CLR.bit.RX_CPU_RTS = 1;
				break;
			case CSK_UART_FLOW_CONTROL_RTS_CTS:
				uart->reg->REG_CTRL.bit.AUTO_FLOW_CONTROL = 1;
				uart->reg->REG_CMD_SET.bit.RX_RTS = 1;
				uart->reg->REG_TRIGGERS.bit.AFC_LEVEL = 63;
				break;
			default:
				return CSK_UART_ERROR_FLOW_CONTROL;
        }
    }

    // Set configured flag
    uart->info->flags |= UART_FLAG_CONFIGURED;

    return CSK_DRIVER_OK;
}

int32_t UART_GetStatus(void *res, CSK_UART_STATUS* stat)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES* uart = (UART_RESOURCES*)res;

    stat->tx_busy = uart->reg->REG_STATUS.bit.TX_ACTIVE;
    stat->rx_busy = uart->info->rx_status.rx_busy;
    stat->tx_underflow = 0U;
    stat->rx_overflow = uart->info->rx_status.rx_overflow;
    stat->rx_break = uart->info->rx_status.rx_break;
    stat->rx_framing_error = uart->info->rx_status.rx_framing_error;
    stat->rx_parity_error = uart->info->rx_status.rx_parity_error;

    return CSK_DRIVER_OK;
}

int32_t UART_SetDMATxChannel(void *res, uint8_t channel)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES *uart = (UART_RESOURCES *)res;

    if (uart->dma_tx == NULL) {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    if (uart->info->xfer.send_active != 0U) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    if ((channel != DMA_CHANNEL_ANY) &&
        (channel >= DMA_MAX_NR_CHANNELS)) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uart->dma_tx->channel = channel;

    return CSK_DRIVER_OK;
}

int32_t UART_SetDMARxChannel(void *res, uint8_t channel)
{
    CHECK_RESOURCES(res);

    UART_RESOURCES *uart = (UART_RESOURCES *)res;

    if (uart->dma_rx == NULL) {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    if (uart->info->rx_status.rx_busy == 1U) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    if ((channel != DMA_CHANNEL_ANY) &&
        (channel >= DMA_MAX_NR_CHANNELS)) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uart->dma_rx->channel = channel;

    return CSK_DRIVER_OK;
}

static uint32_t uart_rxline_irq_handler(UART_RESOURCES *uart)
{
    uint32_t lsr, event;

    event = 0U;
    lsr = uart->reg->REG_STATUS.all;

    if (lsr & UART_RX_OVERFLOW_ERR) {         // RX overflow error
        uart->info->rx_status.rx_overflow = 1U;
        event |= CSK_UART_EVENT_RX_OVERFLOW;
    }
    if (lsr & UART_RX_PARITY_ERR) {          // Parity error
        uart->info->rx_status.rx_parity_error = 1U;
        event |= CSK_UART_EVENT_RX_PARITY_ERROR;
    }
    if (lsr & UART_RX_BREAK_INT) {           // Break detected
        uart->info->rx_status.rx_break = 1U;
        event |= CSK_UART_EVENT_RX_BREAK;
    }
    if (lsr & UART_RX_FRAMING_ERR) {          // Framing error
		uart->info->rx_status.rx_framing_error = 1U;
		event |= CSK_UART_EVENT_RX_FRAMING_ERROR;
    }
    if (lsr & UART_TX_OVERFLOW_ERR) {    // tx overflow error
		event |= CSK_UART_EVENT_TX_OVERFLOW;
	}

    // clear line error
    uart->reg->REG_STATUS.all = 1;

    return event;
}

static void UART_IRQ_Handler(UART_RESOURCES *uart)
{
    uint32_t iir, imsk, val, i, event = 0;
    // Get interrupt status

    iir = uart->reg->REG_IRQ_CAUSE.all;
    imsk = uart->reg->REG_IRQ_MASK.all;

	// Transmit holding register empty, It depend on threshold
	if ((imsk & UART_TX_DATA_NEEDED) && (iir & UART_TX_DATA_NEEDED)) {
		// Get the residue FIFO data
		val = uart->reg->REG_STATUS.bit.TX_FIFO_SPACE;  //for debug -2
		while ((val) && (uart->info->xfer.tx_cnt < uart->info->xfer.tx_num)) {
			hal_SendByte(uart->reg, uart->info->xfer.tx_buf[uart->info->xfer.tx_cnt]);
			uart->info->xfer.tx_cnt++;
			val--;
		}
		// Check if all data is transmitted
		if (uart->info->xfer.tx_num == uart->info->xfer.tx_cnt) {
			// Disable THRE interrupt
			uart->reg->REG_IRQ_MASK.bit.TX_DATA_NEEDED = 0;
			// Clear TX busy flag
			uart->info->xfer.send_active = 0U;
			// Set send complete event
			event |= CSK_UART_EVENT_SEND_COMPLETE;
		}
	}
	// Receive line status, Only trigger line status
	if ((imsk & UART_RX_DATA_AVAILABLE) || (imsk & UART_RX_TIMEOUT)) {
		if(iir & UART_RX_LINE_ERR) {
			event |= uart_rxline_irq_handler(uart);
		}
		// Receive data available and Character time-out indicator interrupt
		if(iir & (UART_RX_DATA_AVAILABLE | UART_RX_TIMEOUT)) {
			// Read RX FIFO residue
			val = uart->reg->REG_STATUS.bit.RX_FIFO_LEVEL;
			if(val + uart->info->xfer.rx_cnt < uart->info->xfer.rx_num) {
				// got partial data
				if(uart->info->timeout) { // timeout enabled
					if(iir & UART_RX_TIMEOUT) { // timeout occur
						event |= CSK_UART_EVENT_RX_TIMEOUT;
					} else { // not occur, reserve a byte for timeout trigger
						val--;
					}
				}
				val += uart->info->xfer.rx_cnt;
			} else {
				val = uart->info->xfer.rx_num;
				event |= CSK_UART_EVENT_RECEIVE_COMPLETE;
			}
			// copy data
			for(i = uart->info->xfer.rx_cnt; i < val; i++) {
				uart->info->xfer.rx_buf[i] = (uint8_t)hal_GetByte(uart->reg);
			}
			uart->info->xfer.rx_cnt = i;
			// finish this receive
			if(event & (CSK_UART_EVENT_RECEIVE_COMPLETE)) {
				uart->reg->REG_IRQ_MASK.all &= ~(UART_RX_DATA_AVAILABLE | UART_RX_TIMEOUT | UART_RX_LINE_ERR);
				// Clear RX busy flag and set receive transfer complete event
				uart->info->rx_status.rx_busy = 0U;
			}
		}
	} else if(iir & UART_RX_DMA_TIMEOUT) { // rx dma timeout
		dma_channel_suspend(uart->dma_rx->channel, 1);
		uart->info->xfer.rx_cnt = dma_channel_get_count(uart->dma_rx->channel);
		//stop dma at first
		dma_channel_disable(uart->dma_rx->channel, 1);
		// copy data
		val = uart->reg->REG_STATUS.bit.RX_FIFO_LEVEL + uart->info->xfer.rx_cnt;
		for(i = uart->info->xfer.rx_cnt; i < val; i++) {
			uart->info->xfer.rx_buf[i] = (uint8_t)hal_GetByte(uart->reg);
		}
		uart->info->xfer.rx_cnt = i;
		//clear irq flag
		uart->reg->REG_IRQ_CAUSE.all = UART_RX_DMA_TIMEOUT;
		uart->reg->REG_IRQ_MASK.all &= ~(UART_RX_DMA_TIMEOUT | UART_RX_LINE_ERR);

		event |= CSK_UART_EVENT_RX_TIMEOUT;
		// Clear RX busy status
		uart->info->rx_status.rx_busy = 0U;
	}
	// call back
	if ((uart->info->cb_event) && (event != 0U)) {
		uart->info->cb_event(event, uart->info->workspace);
	}
#if CONFIG_PM && PM_UART_WAKEUP
	uart_pm_info.state = UART_PM_STATE_BUSY;
	uart_pm_info.active_time = SysTimer_GetLoadValue();
#endif
}

static void UART_DMA_TX_Handler(uint32_t event, UART_RESOURCES *uart)
{
    switch (event) {
    case DMA_EVENT_TRANSFER_COMPLETE:
        uart->info->xfer.tx_cnt = uart->info->xfer.tx_num;
        // Clear TX busy flag
        uart->info->xfer.send_active = 0U;
        // clear dma_tx bit
        uart->reg->REG_IRQ_CAUSE.all = 1;

        // Set Send Complete event for asynchronous transfers
        if (uart->info->cb_event) {
            uart->info->cb_event(CSK_UART_EVENT_SEND_COMPLETE, uart->info->workspace);
        }
        break;
    case DMA_EVENT_ERROR:
        default:
        break;
    }
}

static void UART_DMA_RX_Handler(uint32_t event, UART_RESOURCES *uart)
{
    switch (event ) {
    case DMA_EVENT_TRANSFER_COMPLETE:
        uart->info->xfer.rx_cnt = uart->info->xfer.rx_num;
        uart->info->rx_status.rx_busy = 0U;
        // clear dma_tx bit
        uart->reg->REG_IRQ_CAUSE.all = 1;

        if (uart->info->cb_event) {
            uart->info->cb_event(CSK_UART_EVENT_RECEIVE_COMPLETE, uart->info->workspace);
        }
        break;
    case DMA_EVENT_ERROR:
        default:
        break;
    }
}


/* UART0 static function */
static void UART0_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    UART_DMA_TX_Handler((event & 0xFF), &uart0_resources);
}
static void UART0_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    UART_DMA_RX_Handler((event & 0xFF), &uart0_resources);
}
static void UART0_IRQ_Handler(void)
{
    UART_IRQ_Handler(&uart0_resources);
}

/* UART1 static function */
static void UART1_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    UART_DMA_TX_Handler((event & 0xFF), &uart1_resources);
}
static void UART1_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    UART_DMA_RX_Handler((event & 0xFF), &uart1_resources);
}
static void UART1_IRQ_Handler(void)
{
    UART_IRQ_Handler(&uart1_resources);
}

/* UART1 static function */
static void UART2_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    UART_DMA_TX_Handler((event & 0xFF), &uart2_resources);
}
static void UART2_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    UART_DMA_RX_Handler((event & 0xFF), &uart2_resources);
}
static void UART2_IRQ_Handler(void)
{
    UART_IRQ_Handler(&uart2_resources);
}

#if CONFIG_PM && PM_UART_WAKEUP
_PM_RAM_TEXT static int32_t UART_check_idle(pm_mode_t mode)
{
    int32_t idle = 1;

    vPortEnterCritical();
    if (uart_pm_info.state == UART_PM_STATE_BUSY)
    {
        if ((SysTimer_GetLoadValue() - uart_pm_info.active_time) > PM_UART_IDLE_TIME)
            uart_pm_info.state = UART_PM_STATE_IDLE;
        else
            idle = 0;
    }
    vPortExitCritical();

    return idle;
}
#endif
