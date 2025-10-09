/*
 * Project:      GPT (General Purpose Timer)
 *               Driver definitions
 */

#ifndef __GPT_COMMON_H
#define __GPT_COMMON_H

#include "Driver_Common.h"

#define CSK_DRIVER_VERSION_MAJOR_MINOR(major,minor) (((major) << 8) | (minor))
#define CSK_GPT_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,01)  /* API version */


/*---------------------Control mode for application---------------------------------*/
typedef enum {
  GPT_CHANNEL0                    = 0,                /**< GPT channel 0 transfer complete */
  GPT_CHANNEL1                    = 1,                /**< GPT channel 1 transfer complete */
  GPT_CHANNEL2                    = 2,                /**< GPT channel 2 transfer complete */
  GPT_CHANNEL3                    = 3,                /**< GPT channel 3 transfer complete */
  GPT_CHANNEL4                    = 4,                /**< GPT channel 4 transfer complete */
  GPT_CHANNEL5                    = 5,                /**< GPT channel 5 transfer complete */
  GPT_CHANNEL6                    = 6,                /**< GPT channel 6 transfer complete */
  GPT_CHANNEL7                    = 7,								/**< GPT channel 7 transfer complete */
} GPT_CHANNEL_TYPE;



/****** GPT specific error codes *****/
// hardware resource confliction, change a channel.
#define CSK_GPT_ERROR_HARDWARE_CONFLICTION                 (CSK_DRIVER_ERROR_SPECIFIC - 1)


/****** GPT Event *****/
#define CSK_GPT_EVENT_OVERFLOW      (1UL << 0)  			///< GPT channel overflow
#define CSK_GPT_EVENT_INPUTCAPTURE  (1UL << 1)  			///< GPT channel input capture
#define CSK_GPT_EVENT_LEDC_TX_DONE  (1UL << 2)  			///< GPT channel ledc tx done

/*---------------------Function Interface for application---------------------------*/

/**
 \brief GPT Status
 */
typedef struct _CSK_GPT_STATUS
{
    uint32_t configured :8;         ///< GPT channel state: 1=Configured, 0=Unconfigured
} CSK_GPT_STATUS;



/**
 \fn          void CSK_GPT_SignalEvent (uint32_t event)
 \brief       Signal GPT Events.
 \param[in]   event  \ref GPT_events notification mask
 \return      none
  */
typedef void
(*CSK_GPT_SignalEvent_t)(uint32_t event, void *param);

/**
 \brief GPT Device Driver Capabilities.
 */
typedef struct _CSK_GPT_CAPABILITIES
{
    uint32_t channels :8;  ///< supports GPT channel numbers
} CSK_GPT_CAPABILITIES;

/**
 \brief Access function of the GPT Driver.
 */

/*!
 \brief	 Get GPT0 resource instance
 */
void* GPT0(void);


/*!
 \brief	 Get GPT version
 */
CSK_DRIVER_VERSION GPT_GetVersion(void);


/*!
 \brief	 Get GPT capabilities
 */
CSK_GPT_CAPABILITIES GPT_GetCapabilities(void *pGpt);


/*!
 \brief	 GPT resource initialization
 */
int32_t	GPT_Initialize(void *pGpt, void *user_param);


/*!
 \brief	 GPT resource uninitialize
 */
int32_t GPT_Uninitialize(void *pGpt);


/*!
 \brief	 GPT power control, full speed/low speed
 */
int32_t	GPT_PowerControl(void *pGpt, CSK_POWER_STATE state);


/*!
 \brief		 Get GPT status.
 */
CSK_GPT_STATUS GPT_GetStatus(void *pGpt);


#endif /* __GPT_COMMON_H */
