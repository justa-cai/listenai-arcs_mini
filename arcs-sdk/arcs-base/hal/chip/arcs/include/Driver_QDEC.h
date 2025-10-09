/*
 * Driver_QDEC.h
 *
 * Created on:
 *
 */

#ifndef __DRIVER_QDEC_H
#define __DRIVER_QDEC_H

#ifdef __cplusplus
extern "C" {
#endif


/********************  QDEC_Error_Code QDEC Error Code  **********************************/
#define QDEC_ERR_NONE	   		((uint32_t)0x00000000U)           /*!< No error          */
#define QDEC_ERR_OF_X			((uint32_t)0x00000001U)           /*!< X overflow error  */
#define QDEC_ERR_OF_Y			((uint32_t)0x00000002U)           /*!< Y overflow error  */
#define QDEC_ERR_OF_Z	        ((uint32_t)0x00000004U)           /*!< Z overflow error  */
#define QDEC_ERR_UF_X	        ((uint32_t)0x00000010U)           /*!< X underflow error */
#define QDEC_ERR_UF_Y	        ((uint32_t)0x00000020U)           /*!< Y underflow error */
#define QDEC_ERR_UF_Z	        ((uint32_t)0x00000040U)           /*!< Z underflow error */

/********************  QDEC_AXES QDEC Axes   *********************************************/
#define QDEC_AXES_X             (0x00000001)
#define QDEC_AXES_Y             (0x00000002)
#define QDEC_AXES_Z             (0x00000004)
#define QDEC_AXES_ALL           (QDEC_AXES_X | QDEC_AXES_Y | QDEC_AXES_Z)

/********************  QDEC_INTERRUPT_MASK QDEC Interrupt Mask  **************************/
#define QDEC_INT_MSK_X_OF       (0x00000001)
#define QDEC_INT_MSK_X_UF       (0x00000002)
#define QDEC_INT_MSK_X_EVENT    (0x00000004)
#define QDEC_INT_MSK_Y_OF       (0x00000008)
#define QDEC_INT_MSK_Y_UF       (0x00000010)
#define QDEC_INT_MSK_Y_EVENT    (0x00000020)
#define QDEC_INT_MSK_Z_OF       (0x00000040)
#define QDEC_INT_MSK_Z_UF       (0x00000080)
#define QDEC_INT_MSK_Z_EVENT    (0x00000100)
#define QDEC_INT_MSK_X          (QDEC_INT_MSK_X_OF | QDEC_INT_MSK_X_UF | QDEC_INT_MSK_X_EVENT)
#define QDEC_INT_MSK_Y          (QDEC_INT_MSK_Y_OF | QDEC_INT_MSK_Y_UF | QDEC_INT_MSK_Y_EVENT)
#define QDEC_INT_MSK_Z          (QDEC_INT_MSK_Z_OF | QDEC_INT_MSK_Z_UF | QDEC_INT_MSK_Z_EVENT)
#define QDEC_INT_MSK_FULL       (QDEC_INT_MSK_X | QDEC_INT_MSK_Y | QDEC_INT_MSK_Z)

/********************  QDEC_STATUS QDEC Status  **************************/
#define QDEC_STATUS_X_OF        (0x00000001)
#define QDEC_STATUS_X_UF        (0x00000002)
#define QDEC_STATUS_X_EVENT     (0x00000004)
#define QDEC_STATUS_Y_OF        (0x00000008)
#define QDEC_STATUS_Y_UF        (0x00000010)
#define QDEC_STATUS_Y_EVENT     (0x00000020)
#define QDEC_STATUS_Z_OF        (0x00000040)
#define QDEC_STATUS_Z_UF        (0x00000080)
#define QDEC_STATUS_Z_EVENT     (0x00000100)


typedef enum {
	QDEC_MODE_X1,				/*!< X1 mode */
	QDEC_MODE_X2,				/*!< X2 mode */
	QDEC_MODE_X4,   			/*!< X4 mode */
} QDEC_Mode_t;

typedef enum {
	QDEC_CLK_DIV_DIS = -1,		/*!< Disable clock divider */
	QDEC_CLK_DIV_1 = 0,			/*!< Clock Divider = 1     */
	QDEC_CLK_DIV_2,				/*!< Clock Divider = 2     */
	QDEC_CLK_DIV_4,				/*!< Clock Divider = 4     */
	QDEC_CLK_DIV_8,				/*!< Clock Divider = 8     */
	QDEC_CLK_DIV_16,			/*!< Clock Divider = 16    */
	QDEC_CLK_DIV_32,			/*!< Clock Divider = 32    */
	QDEC_CLK_DIV_64,			/*!< Clock Divider = 64    */
	QDEC_CLK_DIV_128,			/*!< Clock Divider = 128   */
	QDEC_CLK_DIV_256,			/*!< Clock Divider = 256   */
	QDEC_CLK_DIV_512,			/*!< Clock Divider = 512   */
	QDEC_CLK_DIV_1024,			/*!< Clock Divider = 1024  */
	QDEC_CLK_DIV_2048,			/*!< Clock Divider = 2048  */
	QDEC_CLK_DIV_4096,			/*!< Clock Divider = 4096  */
	QDEC_CLK_DIV_8192,			/*!< Clock Divider = 8192  */
	QDEC_CLK_DIV_16384,			/*!< Clock Divider = 16384 */
	QDEC_CLK_DIV_32768,			/*!< Clock Divider = 32768 */
} QDEC_Clk_Div_t;


typedef void (*CSK_QDEC_SignalEvent_t)(uint32_t event, uint32_t param);


/**
  * @brief   QDEC Init structure definition
  */
typedef struct
{
	QDEC_Mode_t	ModeX;                    /*!< Specifies the mode for position counter X */
	QDEC_Mode_t	ModeY;                    /*!< Specifies the mode for position counter Y */
	QDEC_Mode_t	ModeZ;                    /*!< Specifies the mode for position counter Z */
	QDEC_Clk_Div_t	ClkDivIn;             /*!< Specifies the internal clock divider      */
	QDEC_Clk_Div_t	ClkDivDeb;            /*!< Specifies the de-bounce clock divider     */
	uint32_t	IntSel;                   /*!< Specifies the selected interrupt sources  */
	uint32_t	SwapX;                    /*!< Specifies if the A/B phase of X swapped   */
	uint32_t	SwapY;                    /*!< Specifies if the A/B phase of X swapped   */
	uint32_t	SwapZ;                    /*!< Specifies if the A/B phase of X swapped   */
}QDEC_InitTypeDef;

/**
  * @brief  QDEC handle Structure definition
  */
typedef struct __QDEC_HandleTypeDef
{
	void                      *Instance;  /*!< QDEC register base address */
	QDEC_InitTypeDef          Init;       /*!< QDEC parameters            */
	CSK_QDEC_SignalEvent_t    cb_event;   /*!< QDEC callback              */
	volatile uint32_t         ErrorCode;  /*!< QDEC error code            */
}QDEC_HandleTypeDef;


void* QDEC_Instance();
int32_t QDEC_Init(QDEC_HandleTypeDef *qdec_dev, void *callback);
int32_t QDEC_DeInit(QDEC_HandleTypeDef *qdec_dev);
void QDEC_Start(QDEC_HandleTypeDef *qdec_dev, uint32_t axes);
void QDEC_Stop(QDEC_HandleTypeDef *qdec_dev, uint32_t axes);
uint32_t QDEC_GetError(QDEC_HandleTypeDef *qdec_dev);
int16_t QDEC_Read_X(QDEC_HandleTypeDef *qdec_dev);
int16_t QDEC_Read_Y(QDEC_HandleTypeDef *qdec_dev);
int16_t QDEC_Read_Z(QDEC_HandleTypeDef *qdec_dev);
void QDEC_SetEvtThrd_X(QDEC_HandleTypeDef *qdec_dev, uint8_t threshold);
void QDEC_SetEvtThrd_Y(QDEC_HandleTypeDef *qdec_dev, uint8_t threshold);
void QDEC_SetEvtThrd_Z(QDEC_HandleTypeDef *qdec_dev, uint8_t threshold);
void QDEC_ClearCounter(QDEC_HandleTypeDef *qdec_dev, uint32_t axes);


#ifdef __cplusplus
}
#endif


#endif /* __DRIVER_QDEC_H */
