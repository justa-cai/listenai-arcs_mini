/*
 * lib_sdc.h
 *
 *  Created on: 2015�~11��25��
 *      Author: lpwang
 */

#ifndef _LIB_SDC_API_H_
#define _LIB_SDC_API_H_
//#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

//typedef unsigned long long      u64;
//typedef long long               s64;
//typedef unsigned int            u32;
//typedef int                     s32;
//typedef unsigned short          u16;
//typedef short                   s16;
//typedef unsigned char           u8;
//typedef char                    s8;
#ifndef s8
#define s8                      char
#endif
#ifndef u8
#define u8                      uint8_t
#endif
#ifndef s16
#define s16                     int16_t
#endif
#ifndef u16
#define u16                     uint16_t
#endif
#ifndef s32
#define s32                     int32_t
#endif
#ifndef u32
#define u32                     uint32_t
#endif
#ifndef s64
#define s64                     int64_t
#endif
#ifndef u64
#define u64                     uint64_t
#endif

#define SD_0 0
#define SD_1 1

#define SD_HOST_NUM 2

#define CARD_TYPE_UNKNOWN   0
#define MEMORY_CARD_TYPE_SD 1
#define MEMORY_CARD_TYPE_MMC 2
#define SDIO_TYPE_CARD      3
#define MEMORY_SDIO_COMBO   4
#define MEMORY_eMMC         5

#define SDC_OPTION_ENABLE       0x00000001
#define SDC_OPTION_CD_INVERT    0x00000002
#define SDC_OPTION_FIXED        0x00000004
#define SDC_OPTION_SDIO_STD_FUNC    0x00000008
#define SDC_OPTION_SDIO_FORCE_3_3_V  0x00000010

/* 0x2F: SoftRst */
#define SD_SOFTRST_ALL      (1 << 0)
#define SD_SOFTRST_CMD      (1 << 1)
#define SD_SOFTRST_DAT      (1 << 2)

#define FTSDC021_READ_CIS

#ifndef SDHCI_INTR_STS_CARD_INTR
#define SDHCI_INTR_STS_CARD_INTR    (1 << 8)
#endif

#ifdef FTSDC021_READ_CIS
struct sdio_func_tuple
{
    struct sdio_func_tuple* next;
    u8 code;
    u8 size;
    u8 data[256];
};
#endif

typedef struct
{
    u32     num;        /* function number */
    u16     vendor;     /* vendor id */
    u16     device;     /* device id */

    u32     max_blksize;    /* maximum block size */
    u32     cur_blksize;    /* current block size */

    u32     enable_timeout; /* max enable timeout in msec */

    u32     state;      /* function state */

    u32     num_info;   /* number of info strings */
    const s8**        info;     /* info strings */
#ifdef FTSDC021_READ_CIS
    struct sdio_func_tuple* tuples;
#endif
    u8      class;      /* standard interface class */
    u8      enable;
} SDIO_FUNC;


typedef void (*SDIO_ISR_handler)(u16);
typedef void (*SDC_device_handler)(u8);
typedef u32(*SDC_driver_func)(u8);

typedef enum _SD_SD_RESULT
{
    ERR_SD_NO_ERROR = 0,
    ERR_SD_CARD_ERROR,
    ERR_SD_CARD_NOT_EXIST,
    ERR_SD_CARD_LOCK,
    ERR_SD_IP_IDX_ERROR,
    ERR_SD_CLK_IN_ERROR,
    ERR_SD_HOST_RESET_TIMEOUT,
    ERR_SD_CARD_STATUS_ERROR,
    ERR_SD_SEND_COMMAND_TIMEOUT,
    ERR_SD_DATA_CRC_ERROR,
    ERR_SD_RSP_CRC_ERROR,
    ERR_SD_DATA_TIMEOUT,
    ERR_SD_RSP_TIMEOUT,
    ERR_SD_CMD_RSP_ARG_ERROR,
    ERR_SD_DATA_LENGTH_TOO_LONG,
    ERR_SD_WAIT_OVERRUN_TIMEOUT,
    ERR_SD_WAIT_UNDERRUN_TIMEOUT,
    ERR_SD_WAIT_DATA_CRC_TIMEOUT,
    ERR_SD_WAIT_TRANSFER_END_TIMEOUT,
    ERR_SD_WAIT_OPERATION_COMPLETE_TIMEOUT,
    ERR_SD_WAIT_TRANSFER_STATE_TIMEOUT,
    ERR_SD_STATE_CHANGE_TIMEOUT,
    ERR_SD_CARD_IS_BUSY,
    ERR_SD_CID_REGISTER_ERROR,
    ERR_SD_CSD_REGISTER_ERROR,
    ERR_SD_OUT_OF_VOLTAGE_RANGE,
    ERR_SD_OUT_OF_ADDRESS_RANGE,
    ERR_SD_INIT_ERROR,
    ERR_SD_CARD_INITIAL_NOT_COMPLETE,
    ERR_SD_OTHER_ERROR
} SD_RESULT;

#define GM_SDC_ACTION_INIT                  1  // SD host init
#define GM_SDC_ACTION_CARD_SCAN             2  // scan card, in : SD Speed
#define GM_SDC_ACTION_GET_CARD_TYPE         3  // get card type ,out : card type
#define GM_SDC_ACTION_SET_BUS_WIDTH         4  // set bus width, in : bus width
#define GM_SDC_ACTION_DET_INIT              5  // detection init setting
#define GM_SDC_ACTION_DET_HANDLE            6  // detection handle
#define GM_SDC_ACTION_SOFT_RESET            7  // soft reset, in : reset flag
#define GM_SDC_ACTION_IS_CARD_EXIST         8  // is exist , return status
#define GM_SDC_ACTION_IS_CARD_WRITABLE      9  // is writable, return status
#define GM_SDC_ACTION_IS_CARD_INSERT        10 // is insert, return status
#define GM_SDC_ACTION_GET_BLK_LEN           11 // get block length, out : block length
#define GM_SDC_ACTION_GET_BLK_NUM           12 // get block number, out : block number
#define GM_SDC_ACTION_GET_ERASE_SIZE        13 // get erase block size, out : erase block size
#define GM_SDC_ACTION_ENABLE_IRQ            14 // enable specific irq index , in : irq bit mask
#define GM_SDC_ACTION_DISABLE_IRQ           15 // disable specific irq index, in : irq bit mask
#define GM_SDC_ACTION_SDIO_REG_IRQ          16 // register sdio SDCARD IRQ, in : callback function pointer( SDIO_ISR_handler )
#define GM_SDC_ACTION_SDIO_REMOVE_IRQ       17 // register sdio SDCARD IRQ
#define GM_SDC_ACTION_IRQ_SET_INIT          18 // IRQ setting in initial state
#define GM_SDC_ACTION_IRQ_SET_NORMAL        19 // IRQ setting in normal state
#define GM_SDC_ACTION_IS_APP_INIT_DONE      20 // is device application initial done , return status
#define GM_SDC_ACTION_SET_APP_INIT_DONE     21 // set device application initial done
#define GM_SDC_ACTION_SET_ADMA_BUFER        22  // set ADMA buffer address, in : adma buffer address
#define GM_SDC_ACTION_IS_HOST_INIT_DONE     23 //  is SDHost initial done, return status
#define GM_SDC_ACTION_ENTER_IDLE_STATE      24 //  enter idle state
#define GM_SDC_ACTION_CARD_DETECTION        25 //  card detection flow
#define GM_SDC_ACTION_REG_SDCARD_APP_INIT   26 //  register sdcard application initial function, in : callback function pointer(SDC_device_handler)
#define GM_SDC_ACTION_REG_SDIO_APP_INIT     27 //  register sdio application initial function, in : callback function pointer(SDC_device_handler)
#define GM_SDC_ACTION_SDIO_FUNC_SET_BLOCK_SIZE  28 //  set SDIO func block size, func->cur_blksize will be updated
#define GM_SDC_ACTION_SDIO_FUNC_ENABLE          29 //  enable SDIO func
#define GM_SDC_ACTION_SDIO_FUNC_DISABLE         30 //  disable SDIO func
#define GM_SDC_ACTION_SDIO_FUNC_CLAIM_IRQ       31 //  claim irq func
#define GM_SDC_ACTION_SDIO_FUNC_RELEASE_IRQ     32 //  release irq func
#define GM_SDC_ACTION_SDIO_GET_FUNC             33 //  get specific func structure
#define GM_SDC_ACTION_SDIO_GET_FUNC_NUM         34 //  get supported func num
#define GM_SDC_ACTION_PS_SET_INIT_FUNC          35 //  set Restore initial function
#define GM_SDC_ACTION_PS_SET_DETECTION_FUNC     36 //  set Restore detection function



//please add after this , don't change before this
typedef void(*sdc_platform_setting_t)(void);

u32 gm_sdc_api_action(u8 ip_idx, u32 type, void* in, void* out);
u32 gm_sdc_api_erase(u8 ip_idx, u32 addr, u32 cnt);
u32 gm_sdc_api_sdcard_sector_read(u8 ip_idx, u32 sector, u32 cnt, void* buff);
u32 gm_sdc_api_sdcard_sector_write(u8 ip_idx, u32 sector, u32 cnt, void* buff);
u32 gm_sdc_api_sdio_cmd53(u8 ip_idx, u32 write, u8 fn, u32 addr, u32 incr_addr, u32* buf, u32 blocks, u32 blksz);
u32 gm_sdc_api_sdio_cmd52(u8 ip_idx, u32 write, u8 fn, u32 addr, u8 in, u8* out);
u32 gm_api_sdc_platform_init(u32 sdc0_option, u32 sdc1_option, sdc_platform_setting_t setting, u32 card_buffer);


#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_SOC_INCLUDE_LIB_SDC_API_H_ */
