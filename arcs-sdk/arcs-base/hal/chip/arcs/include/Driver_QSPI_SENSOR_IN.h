/*
 * Project:      QSPI_SENSOR_IN (Serial Peripheral Interface) Driver definitions
 */

#ifndef __INCLUDE_DRIVER_QSPI_SENSOR_IN_H
#define __INCLUDE_DRIVER_QSPI_SENSOR_IN_H

#include "Driver_Common.h"

#define CSK_QSPI_SENSOR_IN_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */

/**
  * @brief  QSPI_SENSOR_IN interrupt status enum definition
  */
typedef enum
{
    QSPI_SENSOR_IN_IRQ_EVENT_RXFIFOOR       = 0,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_TXFIFOUR       = 1,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_RXFIFO         = 2,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_TXFIFO         = 3,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_END            = 4,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_SLVCMD         = 5,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_ERR            = 6,   /*!<       */
    QSPI_SENSOR_IN_IRQ_EVENT_FRAME_START    = 7,   /*!< sof   */
    QSPI_SENSOR_IN_IRQ_EVENT_FRAME_END      = 8,   /*!< eof   */
    QSPI_SENSOR_IN_IRQ_EVENT_LINE_END       = 9,   /*!< eol   */
    QSPI_SENSOR_IN_IRQ_EVENT_LINE_START     = 10,  /*!< sol   */
    QSPI_SENSOR_IN_IRQ_EVENT_BUTT
} em_QSPI_SENSOR_IN_IrqEvent;


typedef enum {
    QSPI_SENSOR_IN_SYNC_MODE_SPRD,
    QSPI_SENSOR_IN_SYNC_MODE_MTK,
    QSPI_SENSOR_IN_SYNC_MODE_ALL,
    QSPI_SENSOR_IN_SYNC_MODE_BUTT,
} em_QSPI_SENSOR_IN_SyncMode;

typedef enum {
    QSPI_SENSOR_IN_CPOL0_CPOH0,
    QSPI_SENSOR_IN_CPOL0_CPOH1,
    QSPI_SENSOR_IN_CPOL1_CPOH0,
    QSPI_SENSOR_IN_CPOL1_CPOH1,
    QSPI_SENSOR_IN_CPOL_CPOH_BUTT,
} em_QSPI_SENSOR_IN_CPOL_CPOH;

typedef struct {
    uint32_t sync_code;
    uint8_t sof;           /*!< sync_code0: 0xFF0000AB */
    uint8_t eof;           /*!< sync_code3: 0xFF0000B6 */
    uint8_t sol;           /*!< sync_code1: 0xFF000080 */
    uint8_t eol;           /*!< sync_code2: 0xFF00009D */
} QSPI_SENSOR_IN_SyncCode_t;

typedef struct {
    em_QSPI_SENSOR_IN_SyncMode mode;            /*!< QSPI_IN_MODE_SPRD/ QSPI_IN_MODE_MTK */
    QSPI_SENSOR_IN_SyncCode_t  sync_code;       /*!<  */
    em_QSPI_SENSOR_IN_CPOL_CPOH cp;             /*!< QSPI_IN_CPOL0_CPOH0/01/10/11 */
    uint8_t lane_num;                           /*!< 1/2/4 */
    bool is_lsb;                                /*!< true/false */
    bool data_merge;                            /*!< true/false */
    bool wire_order;                            /*!< true/false */
} QSPI_SENSOR_IN_InitTypeDef;


/**
 \fn          void CSK_DVP_SignalEvent_t (DVP_emIrqEvent event, uint32_t usr_param)
 \brief       Signal DVP Events.
 \param[in]   event        DVP event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void (*QSPI_SENSOR_IN_SignalEvent_t)(em_QSPI_SENSOR_IN_IrqEvent event, uint32_t param);


//------------------------------------------------------------------------------------------
/**
  * @brief  Return DVP driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
QSPI_SENSOR_IN_GetVersion(void);

/**
 * @brief Initialize the QSPI_SENSOR_IN (Digital Video Processor) device.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @param pCallback The callback function for DVP events.
 * @param pCfg The configuration structure for the DVP device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t
QSPI_SENSOR_IN_Initialize(void *pDev, void *pCallback, QSPI_SENSOR_IN_InitTypeDef *pCfg);

/**
 * @brief Uninitializes the DVP device.
 *
 * This function uninitializes the DVP device by performing a reset and disabling the VI.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t
QSPI_SENSOR_IN_Uninitialize(void *pDev);

/**
 * @brief Start the DVP to capture video frames.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
QSPI_SENSOR_IN_Start(void *pDev);

/**
 * @brief Stops the DVP and releases its resources.
 *
 * @param pDvpDev A pointer to the DVP device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t
QSPI_SENSOR_IN_Stop(void *pDev);

/**
  * @brief  Return DVP instance.
  *
  * @return Instance of DVP
  */
void* QSPI_SENSOR_IN0(void);

/**
  * @brief  Return DVP BUF instance.
  *
  * @return Instance of RGB
  */
uint32_t QSPI_SENSOR_IN0_Buf(void);





#endif /* __DRIVER_QSPI_SENSOR_IN_H */
