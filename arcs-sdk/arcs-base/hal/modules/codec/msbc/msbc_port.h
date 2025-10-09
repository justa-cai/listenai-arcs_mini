/**
 ****************************************************************************************
 *
 * @file msbc_port.h
 *
 * @brief msbc port
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef MSBC_PORT_H
#define MSBC_PORT_H

#include "msbc_typedef.h"

#define  AUDIO_TRANSMITTER
#define  AUDIO_RECEIVER

#if (defined AUDIO_TRANSMITTER) && (defined AUDIO_RECEIVER)
  #ifndef MSBC_ENCODE
    #define MSBC_ENCODE
  #endif //MSBC_ENCODE
  #ifndef MSBC_DECODE
    #define MSBC_DECODE
  #endif //MSBC_DECODE
#elif (defined AUDIO_TRANSMITTER)
  #ifndef MSBC_ENCODE
    #define MSBC_ENCODE
  #endif //MSBC_ENCODE
  #ifdef MSBC_DECODE
    #undef MSBC_DECODE
  #endif //MSBC_DECODE
#elif (defined AUDIO_RECEIVER)
  #ifndef MSBC_DECODE
    #define MSBC_DECODE
  #endif //MSBC_DECODE
  #ifdef MSBC_ENCODE
    #undef MSBC_ENCODE
  #endif //MSBC_ENCODE
#endif //(defined AUDIO_TRANSMITTER) && (defined AUDIO_RECEIVER)


#define SBC_EIO       (1)
#define SBC_ENOMEM    (2)
#define SBC_EINVAL    (3)
#define SBC_ENOSPC    (4)
#define SBC_ENLENINV  (5)

//typedef int32_t ssize_t;

#endif //SBC_RDA585X_PORT_H
