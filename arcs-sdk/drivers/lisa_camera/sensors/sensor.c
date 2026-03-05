#include <stdbool.h>
#include <stdint.h>
#include "sensor.h"
#include "lisa_device.h"
#include "lisa_i2c.h"

#define TAG "camera_sensor"
#include "lisa_log.h"

static lisa_device_t *i2c_dev = NULL;

const camera_sensor_info_t camera_sensor[CAMERA_MODEL_MAX] = {
    {CAMERA_OV7725, "OV7725", OV7725_SCCB_ADDR, OV7725_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_OV2640, "OV2640", OV2640_SCCB_ADDR, OV2640_PID, 1600, 1200, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_OV3660, "OV3660", OV3660_SCCB_ADDR, OV3660_PID, 2048, 1536, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_OV5640, "OV5640", OV5640_SCCB_ADDR, OV5640_PID, 2560, 1920, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_OV7670, "OV7670", OV7670_SCCB_ADDR, OV7670_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_NT99141, "NT99141", NT99141_SCCB_ADDR, NT99141_PID, 1280, 720, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_GC2145, "GC2145", GC2145_SCCB_ADDR, GC2145_PID, 1600, 1200, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_GC032A, "GC032A", GC032A_SCCB_ADDR, GC032A_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_GC0328, "GC0328", GC0328_SCCB_ADDR, GC0328_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_GC0310, "GC0310", GC0310_SCCB_ADDR, GC0310_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_GC0308, "GC0308", GC0308_SCCB_ADDR, GC0308_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_BF3005, "BF3005", BF3005_SCCB_ADDR, BF3005_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_BF20A6, "BF20A6", BF20A6_SCCB_ADDR, BF20A6_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_BF3901, "BF3901", BF3901_SCCB_ADDR, BF3901_PID, 320, 240, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_SC101IOT, "SC101IOT", SC101IOT_SCCB_ADDR, SC101IOT_PID, 1280, 720, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_SC030IOT, "SC030IOT", SC030IOT_SCCB_ADDR, SC030IOT_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_SC031GS, "SC031GS", SC031GS_SCCB_ADDR, SC031GS_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
    {CAMERA_OV9655, "OV9655", OV9655_SCCB_ADDR, OV9655_PID, 640, 480, (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
};

camera_sensor_info_t *camera_sensor_get_info(camera_model_t model)
{
    for (int i = 0; i < CAMERA_MODEL_MAX; i++) {
        if (camera_sensor[i].model == model) {
            return (camera_sensor_info_t *)&camera_sensor[i];
        }
    }
    return NULL;
}

int32_t sensor_twi_init(lisa_device_t *dev)
{
    int ret = 0;

    // 获取I2C设备
    i2c_dev = dev;
    if (!i2c_dev || !lisa_device_ready(i2c_dev)) {
        LOGE("I2C device not ready");
        return -1;
    }

    // 配置I2C总线
    lisa_i2c_config_t i2c_cfg = {
        .speed = LISA_I2C_SPEED_FAST,
        .master_mode = true,
        .slave_addr = 0,
    };

    ret = lisa_i2c_configure(i2c_dev, &i2c_cfg);
    if (ret != LISA_DEVICE_OK) {
        LOGE("lisa_i2c_configure failed %d", ret);
        return ret;
    }

    return 0;
}

static int _twi_write_raw8(uint8_t slvaddr, uint8_t *values, int count, bool xfstop)
{
    return lisa_i2c_write(i2c_dev, slvaddr, values, count);
}

static int _twi_write(uint8_t slv_addr, uint16_t reg, unsigned char value, bool reg16_width)
{
    uint8_t buf[3];
    uint32_t num = 0;

    if (reg16_width) {
        buf[0] = ((reg >> 8) & 0xff);
        buf[1] = (reg & 0xff);
        buf[2] = value;
        num = 3;
    }
    else {
        buf[0] = reg;
        buf[1] = value;
        num = 2;
    }

    return lisa_i2c_write(i2c_dev, slv_addr, buf, num);
}

static int _twi_read(uint8_t slv_addr, uint16_t reg, unsigned char *value, bool reg16_width)
{
    lisa_i2c_msg_t msgs[2];
    uint8_t reg_buf[2];
    int reg_len = reg16_width ? 2 : 1;

    // 准备寄存器地址
    if (reg16_width) {
        reg_buf[0] = (reg >> 8) & 0xff;
        reg_buf[1] = reg & 0xff;
    } else {
        reg_buf[0] = reg & 0xff;
    }

    // 消息1：写入寄存器地址（不发送STOP）
    msgs[0].addr = slv_addr;
    msgs[0].flags = LISA_I2C_FLAG_NO_STOP;
    msgs[0].len = reg_len;
    msgs[0].buf = reg_buf;

    // 消息2：读取数据
    msgs[1].addr = slv_addr;
    msgs[1].flags = LISA_I2C_FLAG_READ;
    msgs[1].len = 1;
    msgs[1].buf = value;

    int ret = lisa_i2c_transfer(i2c_dev, msgs, 2);
    if (ret != LISA_DEVICE_OK) {
        LOGE("i2c transfer failed %d", ret);
    }

    return ret;
}

uint8_t sensor_twi_read_reg8(uint8_t slv_addr, uint8_t reg)
{
    int ret = 0;
    uint8_t value = 0;
    int cnt = 0;

    ret = _twi_read(slv_addr, reg, &value, false);
    while (ret != 0 && cnt < 2) {
        CLOGW("twi read retry %d\r\n", cnt);
        ret = _twi_read(slv_addr, reg, &value, false);
        // log....
        cnt++;
    }
    // if (cnt > 0)
        // log...

    return value;
}

uint8_t sensor_twi_read_reg16(uint8_t slv_addr, uint16_t reg)
{
    int ret = 0;
    uint8_t value = 0;
    int cnt = 0;

    ret = _twi_read(slv_addr, reg, &value, true);
    while (ret != 0 && cnt < 2) {
        CLOGW("twi read retry %d\r\n", cnt);
        ret = _twi_read(slv_addr, reg, &value, true);
        // log....
        cnt++;
    }
    // if (cnt > 0)
        // log...

    return value;
}

int sensor_twi_write_reg8(uint8_t slv_addr, uint8_t reg, uint8_t value)
{
    int ret = 0;
    int cnt = 0;

    ret = _twi_write(slv_addr, reg, value, false);
    while (ret != 0 && cnt < 2) {
        ret = _twi_write(slv_addr, reg, value, false);
        cnt++;
        CLOGW("twi write retry %d %d\r\n", cnt, ret);
    }

    return ret;
}

int sensor_twi_write_reg16(uint8_t slv_addr, uint16_t reg, uint8_t value)
{
    int ret = 0;
    int cnt = 0;

    ret = _twi_write(slv_addr, reg, value, true);
    while (ret != 0 && cnt < 2) {
        ret = _twi_write(slv_addr, reg, value, true);
        cnt++;
        CLOGW("twi write retry %d %d\r\n", cnt, ret);
    }

    return ret;
}

int sensor_twi_write_raw8(uint8_t slvaddr, uint8_t *values, int count)
{
    return _twi_write_raw8(slvaddr, values, count, true);
}

// int sensor_twi_write_array(uint8_t slv_addr, struct regval_list *regs, int array_size)
// {
//     int ret = 0;
//     int i = 0;

//     if (!regs)
//         return -1;

//     while (i < array_size) {
//         if (regs->addr == REG_DLY) {
//             // hal_msleep(regs->data);
//         }
//         else {
//             ret = sensor_twi_write_reg8(slv_addr, regs->addr, regs->data);
//             if (ret != 0) {
//                 // Log....
//                 CLOGE("spi_sensor_twi_write failed %d\r\n", ret);
//                 return ret;
//             }
//         }
//         i++;
//         regs++;
//     }
//     return ret;
// }
