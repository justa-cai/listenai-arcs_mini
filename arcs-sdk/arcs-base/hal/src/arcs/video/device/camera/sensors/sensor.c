#include <stdio.h>
#include "sensor.h"

const camera_sensor_info_t camera_sensor[CAMERA_MODEL_MAX] = {
    // The sequence must be consistent with camera_model_t
    {CAMERA_OV7725, "OV7725", OV7725_SCCB_ADDR, OV7725_PID, FRAMESIZE_VGA, false},
    {CAMERA_OV2640, "OV2640", OV2640_SCCB_ADDR, OV2640_PID, FRAMESIZE_UXGA, true},
    {CAMERA_OV3660, "OV3660", OV3660_SCCB_ADDR, OV3660_PID, FRAMESIZE_QXGA, true},
    {CAMERA_OV5640, "OV5640", OV5640_SCCB_ADDR, OV5640_PID, FRAMESIZE_QSXGA, true},
    {CAMERA_OV7670, "OV7670", OV7670_SCCB_ADDR, OV7670_PID, FRAMESIZE_VGA, false},
    {CAMERA_NT99141, "NT99141", NT99141_SCCB_ADDR, NT99141_PID, FRAMESIZE_HD, true},
    {CAMERA_GC2145, "GC2145", GC2145_SCCB_ADDR, GC2145_PID, FRAMESIZE_UXGA, false},
    {CAMERA_GC032A, "GC032A", GC032A_SCCB_ADDR, GC032A_PID, FRAMESIZE_VGA, false},
    {CAMERA_GC0328, "GC0328", GC0328_SCCB_ADDR, GC0328_PID, FRAMESIZE_VGA, false},
    {CAMERA_GC0310, "GC0310", GC0310_SCCB_ADDR, GC0310_PID, FRAMESIZE_VGA, false},
    {CAMERA_GC0308, "GC0308", GC0308_SCCB_ADDR, GC0308_PID, FRAMESIZE_VGA, false},
    {CAMERA_BF3005, "BF3005", BF3005_SCCB_ADDR, BF3005_PID, FRAMESIZE_VGA, false},
    {CAMERA_BF20A6, "BF20A6", BF20A6_SCCB_ADDR, BF20A6_PID, FRAMESIZE_VGA, false},
    {CAMERA_SC101IOT, "SC101IOT", SC101IOT_SCCB_ADDR, SC101IOT_PID, FRAMESIZE_HD, false},
    {CAMERA_SC030IOT, "SC030IOT", SC030IOT_SCCB_ADDR, SC030IOT_PID, FRAMESIZE_VGA, false},
    {CAMERA_SC031GS, "SC031GS", SC031GS_SCCB_ADDR, SC031GS_PID, FRAMESIZE_VGA, false},
    {CAMERA_OV9655, "OV9655", OV9655_SCCB_ADDR, OV9655_PID, FRAMESIZE_VGA, false},
};

const resolution_info_t resolution[FRAMESIZE_INVALID] = {
    {   96,   96, ASPECT_RATIO_1X1   }, /* 96x96 */
    {  160,  120, ASPECT_RATIO_4X3   }, /* QQVGA */
    {  176,  144, ASPECT_RATIO_5X4   }, /* QCIF  */
    {  240,  176, ASPECT_RATIO_4X3   }, /* HQVGA */
    {  240,  240, ASPECT_RATIO_1X1   }, /* 240x240 */
    {  320,  240, ASPECT_RATIO_4X3   }, /* QVGA  */
    {  400,  296, ASPECT_RATIO_4X3   }, /* CIF   */
    {  480,  320, ASPECT_RATIO_3X2   }, /* HVGA  */
    {  640,  480, ASPECT_RATIO_4X3   }, /* VGA   */
    {  800,  600, ASPECT_RATIO_4X3   }, /* SVGA  */
    { 1024,  768, ASPECT_RATIO_4X3   }, /* XGA   */
    { 1280,  720, ASPECT_RATIO_16X9  }, /* HD    */
    { 1280, 1024, ASPECT_RATIO_5X4   }, /* SXGA  */
    { 1600, 1200, ASPECT_RATIO_4X3   }, /* UXGA  */
    // 3MP Sensors
    { 1920, 1080, ASPECT_RATIO_16X9  }, /* FHD   */
    {  720, 1280, ASPECT_RATIO_9X16  }, /* Portrait HD   */
    {  864, 1536, ASPECT_RATIO_9X16  }, /* Portrait 3MP   */
    { 2048, 1536, ASPECT_RATIO_4X3   }, /* QXGA  */
    // 5MP Sensors
    { 2560, 1440, ASPECT_RATIO_16X9  }, /* QHD    */
    { 2560, 1600, ASPECT_RATIO_16X10 }, /* WQXGA  */
    { 1088, 1920, ASPECT_RATIO_9X16  }, /* Portrait FHD   */
    { 2560, 1920, ASPECT_RATIO_4X3   }, /* QSXGA  */
};

camera_sensor_info_t *camera_sensor_get_info(camera_model_t model)
{
    for (int i = 0; i < CAMERA_MODEL_MAX; i++)
    {
        if (model == camera_sensor[i].model)
        {
            return (camera_sensor_info_t *)&camera_sensor[i];
        }
    }
    return NULL;
}


static sensor_port_callback_t *sensor_callback;

int camera_sensor_port_callback(sensor_port_callback_t *callback)
{
    if(NULL != callback)
    {
        sensor_callback = callback;
        return 0;
    }
    return -1;
}


uint8_t camera_sensor_read_reg8(uint8_t slv_addr, uint8_t reg)
{
    if(NULL != sensor_callback->read_reg8)
    {
        return sensor_callback->read_reg8(slv_addr, reg);
    }
    return 0;
}


uint8_t camera_sensor_read_reg16(uint8_t slv_addr, uint16_t reg)
{
    if(NULL != sensor_callback->read_reg16)
    {
        return sensor_callback->read_reg16(slv_addr, reg);
    }
    return 0;
}


int camera_sensor_write_reg8(uint8_t slv_addr, uint8_t reg, uint8_t value)
{
    if(NULL != sensor_callback->write_reg8)
    {
        sensor_callback->write_reg8(slv_addr, reg, value);
    }
    return 0;
}


int camera_sensor_write_reg16(uint8_t slv_addr, uint16_t reg, uint8_t value)
{
    if(NULL != sensor_callback->write_reg16)
    {
        sensor_callback->write_reg16(slv_addr, reg, value);
    }
    return 0;
}


void camera_sensor_delay_ms(uint32_t nms)
{
    if(NULL != sensor_callback->delay_ms)
    {
        sensor_callback->delay_ms(nms);
    }
}


void camera_sensor_delay_us(uint32_t nus)
{
    if(NULL != sensor_callback->delay_us)
    {
        sensor_callback->delay_us(nus);
    }
}


int camera_sensor_log(const char* format, ...)
{
    if(NULL != sensor_callback->log)
    {
        sensor_callback->log(format);
    }
    return 0;
}


