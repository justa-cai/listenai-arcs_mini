#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcs_ap.h"
#include "log_print.h"
#include "sdiod_reg.h"
#include "sdio_device.h"
#include "Driver_Common.h"
#include "ClockManager.h"

#define SDIO_DEVICE_LOGE(s, ...) //CLOGE("%s:%d (%s):"s, __FILE__,__LINE__,__FUNCTION__,##__VA_ARGS__)
#define SDIO_DEVICE_LOGW(s, ...) //CLOGW("%s: "s, __FUNCTION__,##__VA_ARGS__)
#define SDIO_DEVICE_LOGD(s, ...) //CLOGD("%s: "s, __FUNCTION__,##__VA_ARGS__)


_FAST_DATA_VI static SDIOD_HandleTypeDef sdiod_dev = {
    IP_SDIOD,
    NULL,
    NULL,
    {0},
    0,
    0,
};


/**
 * @brief Get the SDIOD instance.
 * @return Pointer to the SDIOD handle.
 */
void* SDIOD_Instance()
{
    return &sdiod_dev;
}

/**
 * @brief SDIOD interrupt handler.
 */
void SDIOD_Handler (void) 
{
    unsigned int irq_cause ;
    irq_cause =  sdiod_dev.Instance->REG_SMID_INT_STAT.all;
    SDIO_DEVICE_LOGD("interrupt = 0x%x\n", irq_cause);

    if((irq_cause & BITSET_0X03C_PROGRAM_START) == BITSET_0X03C_PROGRAM_START) {
        SDIO_DEVICE_LOGD("program start\n");
        sdiod_dev.Instance->REG_SMID_INT_STAT.all = BITSET_0X03C_PROGRAM_START;
    }

	if((irq_cause & BITSET_0X03C_WRITE_START_INTERRUPT) == BITSET_0X03C_WRITE_START_INTERRUPT) {
		SDIO_DEVICE_LOGD("write start\n");
		sdiod_dev.cmd_info.dir = SDIO_TRANS_HOST2DEV;
		sdiod_dev.cmd_info.cmd_idx = sdiod_dev.Instance->REG_SMID_CMD_REG.bit.CMD_INDEX;
		sdiod_dev.cmd_info.block_size = sdiod_dev.Instance->REG_SMID_CMD_REG.bit.BLOCK_SIZE;
		sdiod_dev.cmd_info.block_cnt = sdiod_dev.Instance->REG_SMID_BLK_CNT.bit.BLOCK_COUNT;

		// 000-32Bytes 001-64Bytes 010-128Bytes 011-256Bytes 100 - 512Bytes 101-1KBytes 110-2KBytes 111-Reserved
		// diod_dev.Instance->REG_SMID_CONTROL_REG.bit.ST_TH_SIZE = (sdiod_dev.cmd_info.block_size >> 6);
		sdiod_dev.Instance->REG_SMID_INT_STAT.all = BITSET_0X03C_WRITE_START_INTERRUPT;

		if(sdiod_dev.buf_used)
		{
			if(sdiod_dev.buf_used->data_recv)
			{
				sdiod_dev.Instance->REG_SMID_DMA1_ADDR.all = (uint32_t)sdiod_dev.buf_used->data_recv;
				sdiod_dev.Instance->REG_SMID_DMA1_CTRL.bit.DMA1_ADDRESS_VALID = 0x1;
			}
		}
	}

	if((irq_cause & BITSET_0X03C_READ_START_INTERRUPT)== BITSET_0X03C_READ_START_INTERRUPT) {
		SDIO_DEVICE_LOGD("read start\n");
		sdiod_dev.cmd_info.dir = SDIO_TRANS_DEV2HOST;
		sdiod_dev.cmd_info.cmd_idx = sdiod_dev.Instance->REG_SMID_CMD_REG.bit.CMD_INDEX;
		sdiod_dev.cmd_info.block_size = sdiod_dev.Instance->REG_SMID_CMD_REG.bit.BLOCK_SIZE;
		sdiod_dev.cmd_info.block_cnt = sdiod_dev.Instance->REG_SMID_BLK_CNT.bit.BLOCK_COUNT;

		// 000-32Bytes 001-64Bytes 010-128Bytes 011-256Bytes 100 - 512Bytes 101-1KBytes 110-2KBytes 111-Reserved
		// sdiod_dev.Instance->REG_SMID_CONTROL_REG.bit.ST_TH_SIZE = (sdiod_dev.cmd_info.block_size >> 6);
		sdiod_dev.Instance->REG_SMID_INT_STAT.all = BITSET_0X03C_READ_START_INTERRUPT;

		if(sdiod_dev.buf_used)
		{
			if(sdiod_dev.buf_used->data_send)
			{
				sdiod_dev.Instance->REG_SMID_DMA1_ADDR.all = (uint32_t)sdiod_dev.buf_used->data_send;
				sdiod_dev.Instance->REG_SMID_DMA1_CTRL.bit.DMA1_ADDRESS_VALID = 0x1;
			}
		}
	}

	if((irq_cause & BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT)== BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT) {
		SDIO_DEVICE_LOGD("transfer complete\n");
		sdiod_dev.Instance->REG_SMID_INT_STAT.all = BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT;
	}

	if(irq_cause & ~(BITSET_0X03C_PROGRAM_START | BITSET_0X03C_WRITE_START_INTERRUPT | BITSET_0X03C_READ_START_INTERRUPT | BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT)) {
		SDIO_DEVICE_LOGD("not handled interrupts\n");
		sdiod_dev.Instance->REG_SMID_INT_STAT.all = irq_cause;
	}

    if(sdiod_dev.cb_event)
        sdiod_dev.cb_event(irq_cause);
}

/**
 * @brief Initialize the SDIO device.
 * @param config Pointer to the SDIO device configuration.
 * @return Pointer to the SDIOD handle.
 */
void *sdio_device_initialize(sdio_device_config_t *config)
{
    SDIOD_HandleTypeDef *sdio_dev = SDIOD_Instance();

    sdio_dev->buf_used = config->buf_info;
    sdio_dev->cb_event = config->event_cb;
    sdio_dev->ErrorCode = SDIOD_ERR_NONE;
    sdio_dev->flags = config->flags;

    __HAL_CRM_SMID_CLK_ENABLE();

    sdio_dev->Instance->REG_SMID_INT_SIG_EN.all = (SDIOD_SMID_INT_SIG_EN_TRANSFER_COMPLETE_INTERRUPT_Msk | SDIOD_SMID_INT_SIG_EN_DMA1_INTERRUPT_Msk \
    		| SDIOD_SMID_INT_SIG_EN_WRITE_START_INTERRUPT_Msk | SDIOD_SMID_INT_SIG_EN_READ_START_INTERRUPT_Msk | SDIOD_SMID_INT_SIG_EN_PROGRAM_START_Msk \
			| SDIOD_SMID_INT_SIG_EN_FUNCTIONX_CRC_ERR_INTERRUPT_Msk | SDIOD_SMID_INT_SIG_EN_FUNCTIONX_ABORT_INTERRUPT_Msk);


    register_ISR(IRQ_SDIOD_VECTOR, SDIOD_Handler, NULL);
    enable_IRQ(IRQ_SDIOD_VECTOR);

    return sdio_dev;
}

/**
 * @brief Deinitialize the SDIO device.
 */
void sdio_device_deinit()
{
    disable_IRQ(IRQ_SDIOD_VECTOR);
    sdiod_dev.ErrorCode = SDIOD_ERR_NONE;
}

/**
 * @brief Start the SDIO device.
 * @return CSK_DRIVER_OK
 */
int sdio_device_start()
{
    sdiod_dev.Instance->REG_SMID_CARD_RDY.bit.FUNCTION1_READY = 0x1; //enable io1
    sdiod_dev.Instance->REG_SMID_CARD_RDY.bit.FUNCTION2_READY = 0x1; //enable io2

    sdiod_dev.Instance->REG_SMID_CONTROL_REG.bit.CARD_INIT_DONE = 0x1;

    return CSK_DRIVER_OK;
}

/**
 * @brief Stop the SDIO device.
 * @return CSK_DRIVER_OK
 */
int sdio_device_stop()
{
    sdiod_dev.Instance->REG_SMID_CARD_RDY.bit.FUNCTION1_READY = 0x0; //disable io1
    sdiod_dev.Instance->REG_SMID_CARD_RDY.bit.FUNCTION2_READY = 0x0; //disable io2

    return CSK_DRIVER_OK;
}

int sdio_device_rx_notify(uint32_t code)
{
    SDIO_DEVICE_LOGD("rx notify\n");
#ifdef GPIO_BASED_PROF
    TOGGLE_GPIO(20, 0);
#endif
    sdiod_dev.Instance->REG_SMID_RD_FN1_TXFRCNT.bit.FUNCTION1_READ_COUNT = (code & 0xFFFF);
#ifdef GPIO_BASED_PROF
    TOGGLE_GPIO(20, 1);
#endif
    return CSK_DRIVER_OK;
}
