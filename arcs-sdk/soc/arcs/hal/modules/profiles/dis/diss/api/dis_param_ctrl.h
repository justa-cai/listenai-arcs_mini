#pragma once


typedef enum
{
    DIS_PARAM_MANUFACTURER_NAME,
    DIS_PARAM_MODEL_NUMBER,
    DIS_PARAM_SERIAL_NUMBER,
    DIS_PARAM_SYSTEM_ID,
    DIS_PARAM_HARDWARE_VERSION,
    DIS_PARAM_SOFTWARE_VERSION,
    DIS_PARAM_FIRWARE_VERSION,
    DIS_PARAM_PNP_ID,
}E_DIS_PARAM;

unsigned short dis_get_param(E_DIS_PARAM type, unsigned char *data, unsigned short max_len);
