#ifndef __SDIO_DEVICE_H_
#define __SDIO_DEVICE_H_

#include "sdiod_reg.h"
#include "arcs_ap.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT
#define BITSET_0X03C_TRANSFER_COMPLETE_INTERRUPT    0x1
#endif

#ifndef BITSET_0X03C_WRITE_START_INTERRUPT
#define BITSET_0X03C_WRITE_START_INTERRUPT          0x8
#endif

#ifndef BITSET_0X03C_READ_START_INTERRUPT
#define BITSET_0X03C_READ_START_INTERRUPT           0x10
#endif

#ifndef BITSET_0X03C_PROGRAM_START
#define BITSET_0X03C_PROGRAM_START                  0x2000000
#endif

#ifndef BITSET_0X03C_LRST_INTERRUPT
#define BITSET_0X03C_LRST_INTERRUPT                 0x40000000
#endif


#define SDIOD_ERR_NONE	   	    (0x00000000U)           /*!< No error          */
#define SDIOD_ERR	            (0x00000001U)           /*!< Error             */

#define SDIO_DEVICE_RECV_MAX_BUFFER  (4096-4)
#define ALIGNx_HI(val, x) (((val) + ((x) - 1)) & ~((x) - 1))

typedef void (*CSK_SDIOD_SignalEvent_t)(uint32_t event);

typedef enum {
    SDIO_TRANS_HOST2DEV = 0,
	SDIO_TRANS_DEV2HOST = 1,
} sdio_trans_t;

#define SDIO_FLAG_WIFI_FUNC_LSB 0
#define SDIO_FLAG_WIFI_FUNC_WIDTH 1
#define SDIO_FLAG_WIFI_FUNC_MASK 0x1
#define SDIO_FLAG_SG_SUPPORT_LSB 1
#define SDIO_FLAG_SG_SUPPORT_WIDTH 1
#define SDIO_FLAG_SG_SUPPORT_MASK 0x2
#define SDIO_FLAG_DMA_BOUND_LSB 2
#define SDIO_FLAG_DMA_BOUND_WIDTH 2
#define SDIO_FLAG_DMA_BOUND_MASK 0xC
#define SDIO_DMA_BOUND_1K 0
#define SDIO_DMA_BOUND_2K 1
#define SDIO_DMA_BOUND_4K 2
#define SDIO_DMA_BOUND_8K 3
#define SDIO_IN_WIFI_MODE(flags) (flags & SDIO_FLAG_WIFI_FUNC_MASK)
#define SDIO_SG_SUPPORT(flags) (flags & SDIO_FLAG_SG_SUPPORT_MASK)
#define SDIO_DMA_BOUND(flags) ((flags & SDIO_FLAG_DMA_BOUND_MASK) >> SDIO_FLAG_DMA_BOUND_LSB)

#define GPIO_BASED_PROF
#ifdef GPIO_BASED_PROF
#define TOGGLE_GPIO(num, value) do { \
            IP_CMN_IOMUX->REG_PAD_GPIOA_##num.bit.PAD_GPIOA_##num##_OUT_FRC = 1; \
            IP_CMN_IOMUX->REG_PAD_GPIOA_##num.bit.PAD_GPIOA_##num##_OEN_FRC = 1; \
            IP_CMN_IOMUX->REG_PAD_GPIOA_##num.bit.PAD_GPIOA_##num##_OEN_REG = 0; \
            IP_CMN_IOMUX->REG_PAD_GPIOA_##num.bit.PAD_GPIOA_##num##_OUT_REG = value; \
        } while(0);
#endif

typedef struct {
    volatile uint8_t  cmd_idx;
    volatile uint8_t  dir;
    volatile uint16_t block_size;
    volatile uint32_t block_cnt;
} sdio_cmd_t;

typedef struct {
    uint8_t* data_send;
    uint8_t* data_recv;
    volatile int size_send;
    volatile int size_recv;
} sdio_buf_t;

typedef struct {
	sdio_buf_t*         buf_info;
    int                 send_buffer_size;
    int                 recv_buffer_size;
    void*               event_cb;
    uint32_t            flags;
} sdio_device_config_t;

typedef struct __SDIOD_HandleTypeDef
{
    SDIOD_RegDef             *Instance;  /*!< register base address */
    CSK_SDIOD_SignalEvent_t   cb_event;  /*!< callback              */
    sdio_buf_t*               buf_used;
    sdio_cmd_t                cmd_info;
    volatile uint32_t         ErrorCode; /*!< error code            */
    uint32_t                  flags;
} SDIOD_HandleTypeDef;

void* SDIOD_Instance();
void *sdio_device_initialize(sdio_device_config_t *config);
int sdio_device_start();
int sdio_device_stop();
void sdio_device_deinit();
int sdio_device_rx_notify(uint32_t code);

#ifdef __cplusplus
}
#endif

#endif /*__SDIO_DEVICE_H */


