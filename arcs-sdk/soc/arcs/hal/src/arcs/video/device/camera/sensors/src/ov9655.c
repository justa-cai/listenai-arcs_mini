// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sensor.h"
#include "ov9655.h"
#include "ov9655_regs.h"
#include "ov9655_settings.h"


#define TAG  "ov9655: "

#define H8(v) ((v)>>8)
#define L8(v) ((v)&0xff)

//#define REG_DEBUG_ON

static int read_reg(uint8_t slv_addr, const uint16_t reg)
{
    int ret = CAMERA_READ_REG8(slv_addr, reg);
#ifdef REG_DEBUG_ON
    if (ret < 0) {
        CAMERA_LOGE(TAG"READ REG 0x%04x FAILED: %d", reg, ret);
    }
#endif
    return ret;
}

static int write_reg(uint8_t slv_addr, const uint16_t reg, uint8_t value)
{
    int ret = 0;
#ifndef REG_DEBUG_ON
    ret = CAMERA_WRITE_REG8(slv_addr, reg, value);
#else
    int old_value = read_reg(slv_addr, reg);
    if (old_value < 0) {
        return old_value;
    }
    if ((uint8_t)old_value != value) {
        CAMERA_LOGI(TAG"NEW REG 0x%04x: 0x%02x to 0x%02x", reg, (uint8_t)old_value, value);
        ret = CAMERA_WRITE_REG8(slv_addr, reg, value);
    } else {
        CAMERA_LOGD(TAG"OLD REG 0x%04x: 0x%02x", reg, (uint8_t)old_value);
        ret = CAMERA_WRITE_REG8(slv_addr, reg, value);//maybe not?
    }
    if (ret < 0) {
        CAMERA_LOGE(TAG"WRITE REG 0x%04x FAILED: %d", reg, ret);
    }
#endif
    return ret;
}

static int check_reg_mask(uint8_t slv_addr, uint16_t reg, uint8_t mask)
{
    return (read_reg(slv_addr, reg) & mask) == mask;
}

static void print_regs(uint8_t slv_addr)
{
#ifdef DEBUG_PRINT_REG
    CAMERA_DELAY_MS(100);
    CAMERA_LOGI(TAG"REG list look ======================");
    for (size_t i = 0xf0; i <= 0xfe; i++) {
        CAMERA_LOGI(TAG"reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }
    CAMERA_LOGI(TAG"\npage 0 ===");
    write_reg(slv_addr, 0xfe, 0x00); // page 0
    for (size_t i = 0x03; i <= 0x24; i++) {
        CAMERA_LOGI(TAG"p0 reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }
    for (size_t i = 0x40; i <= 0x95; i++) {
        CAMERA_LOGI(TAG"p0 reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }
    CAMERA_LOGI(TAG"\npage 3 ===");
    write_reg(slv_addr, 0xfe, 0x03); // page 3
    for (size_t i = 0x01; i <= 0x43; i++) {
        CAMERA_LOGI(TAG"p3 reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }
#endif
}

static int set_reg_bits(uint8_t slv_addr, uint16_t reg, uint8_t offset, uint8_t mask, uint8_t value)
{
    int ret = 0;
    uint8_t c_value, new_value;
    ret = read_reg(slv_addr, reg);
    if (ret < 0) {
        return ret;
    }
    c_value = ret;
    new_value = (c_value & ~(mask << offset)) | ((value & mask) << offset);
    ret = write_reg(slv_addr, reg, new_value);
    return ret;
}

static int write_regs(uint8_t slv_addr, const uint8_t (*regs)[2], size_t regs_size)
{
    int i = 0, ret = 0;
    while (!ret && (i < regs_size)) {
        if (regs[i][0] == REG_DLY) {
            CAMERA_DELAY_MS(regs[i][1]);
        } else {
            ret = write_reg(slv_addr, regs[i][0], regs[i][1]);
        }
        i++;
    }
    return ret;
}


static int reset(sensor_t *sensor)
{
    int ret;

    // Software Reset: clear all registers and reset them to their default values
    ret = write_reg(sensor->slv_addr, OV9655_REG_COM7, SCCB_REG_RESET);
    if (ret) {
        CAMERA_LOGE(TAG"Software Reset FAILED!");
        return ret;
    }
    CAMERA_DELAY_MS(100);

    ret = write_regs(sensor->slv_addr, ov9655_default_regs, sizeof(ov9655_default_regs)/(sizeof(uint8_t) * 2));
    if (ret == 0) {
        /* Set the RGB565 mode */
        write_reg(sensor->slv_addr, OV9655_REG_COM7, FORMAT_CTRL_30fpsVGA_VArioPixel | OUTPUT_FORMAT_RGB);
        write_reg(sensor->slv_addr, OV9655_REG_COM15, RGB_565);

        /* Invert the HRef signal*/
        write_reg(sensor->slv_addr, OV9655_REG_COM10, 0x08);

        CAMERA_LOGD(TAG"Camera defaults loaded");
        CAMERA_DELAY_MS(100);
        write_reg(sensor->slv_addr, OV9655_REG_COM7, 0x00);
    }

    return ret;
}


static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret = 0;

    switch (pixformat)
    {
        case PIXFORMAT_RGB565:
            ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM7, 0, 0x3, 0x3);
            ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM15, 4, 0x3, 0x1);
            break;

        case PIXFORMAT_RGB555:
            ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM7, 0, 0x3, 0x3);
            ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM15, 4, 0x3, 0x3);
            break;

        case PIXFORMAT_YUV422:
            ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM7, 0, 0x3, 0x2);
            break;

        default:
            CAMERA_LOGW(TAG"unsupport format");
            ret = -1;
            break;
    }

    if (ret == 0) {
        sensor->pixformat = pixformat;
        CAMERA_LOGD(TAG"Set pixformat to: %u", pixformat);
    }

    return ret;
}


static int set_framesize(sensor_t *sensor, framesize_t framesize)
{
    CAMERA_LOGW(TAG"Unsupported");
    return -1;
}


static int set_hmirror(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.hmirror = enable;
    ret |= set_reg_bits(sensor->slv_addr, OV9655_REG_MVFP, 5, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set h-mirror to: %d", enable);
    }
    return ret;
}


static int set_vflip(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.vflip = enable;
    ret |= set_reg_bits(sensor->slv_addr, OV9655_REG_MVFP, 4, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set v-flip to: %d", enable);
    }
    return ret;
}


static int set_colorbar(sensor_t *sensor, int enable)
{
    int ret = 0;

    ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM3, 7, 0x01, enable);
    ret += set_reg_bits(sensor->slv_addr, OV9655_REG_COM20, 4, 0x01, enable);
    if (ret == 0) {
        sensor->status.colorbar = enable;
        CAMERA_LOGD(TAG"Set colorbar to: %d", enable);
    }

    return ret;
}


static int get_reg(sensor_t *sensor, int reg, int mask)
{
    int ret = 0;
    if (mask > 0xFF) {
        CAMERA_LOGE(TAG"mask should not more than 0xff");
    } else {
        ret = read_reg(sensor->slv_addr, reg);
    }
    if (ret > 0) {
        ret &= mask;
    }
    return ret;
}


static int set_reg(sensor_t *sensor, int reg, int mask, int value)
{
    int ret = 0;
    if (mask > 0xFF) {
        CAMERA_LOGE(TAG"mask should not more than 0xff");
    } else {
        ret = read_reg(sensor->slv_addr, reg);
    }
    if (ret < 0) {
        return ret;
    }
    value = (ret & ~mask) | (value & mask);

    if (mask > 0xFF) {

    } else {
        ret = write_reg(sensor->slv_addr, reg, value);
    }
    return ret;
}


static int init_status(sensor_t *sensor)
{
    sensor->status.brightness = 0;
    sensor->status.contrast = 0;
    sensor->status.saturation = 0;
    sensor->status.sharpness = 0;
    sensor->status.denoise = 0;
    sensor->status.ae_level = 0;
    sensor->status.gainceiling = 0;
    sensor->status.awb = 0;
    sensor->status.dcw = 0;
    sensor->status.agc = 0;
    sensor->status.aec = 0;
    sensor->status.hmirror = check_reg_mask(sensor->slv_addr, OV9655_REG_MVFP, 0x20);
    sensor->status.vflip = check_reg_mask(sensor->slv_addr, OV9655_REG_MVFP, 0x10);
    sensor->status.colorbar = 0;
    sensor->status.bpc = 0;
    sensor->status.wpc = 0;
    sensor->status.raw_gma = 0;
    sensor->status.lenc = 0;
    sensor->status.quality = 0;
    sensor->status.special_effect = 0;
    sensor->status.wb_mode = 0;
    sensor->status.awb_gain = 0;
    sensor->status.agc_gain = 0;
    sensor->status.aec_value = 0;
    sensor->status.aec2 = 0;
    return 0;
}


static int set_dummy(sensor_t *sensor, int val)
{
    CAMERA_LOGW(TAG"Unsupported");
    return -1;
}


static int set_gainceiling_dummy(sensor_t *sensor, gainceiling_t val)
{
    CAMERA_LOGW(TAG"Unsupported");
    return -1;
}


int ov9655_detect(int slv_addr, sensor_id_t *id)
{
#if 1
    if (OV9655_SCCB_ADDR == slv_addr) {
        uint8_t PID = read_reg(slv_addr, OV9655_REG_PID);
        if (OV9655_PID == PID) {
            id->PID = PID;
            return PID;
        } else {
            CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
        }
    }
#else
    for(slv_addr = 0; slv_addr <= 0x7F; slv_addr++)
    {
        CAMERA_LOGI(TAG"slv_addr=0x%x", slv_addr);
        uint8_t PID = read_reg(slv_addr, OV9655_REG_PID);
        if (OV9655_PID == PID) {
            id->PID = PID;
            CAMERA_LOGI(TAG"ov9655_detect");
            return PID;
        } else {
            CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
        }
    }
#endif
    return 0;
}


int ov9655_init(sensor_t *sensor)
{
    sensor->init_status = init_status;
    sensor->reset = reset;
    sensor->set_pixformat = set_pixformat;
    sensor->set_framesize = set_framesize;
    sensor->set_contrast = set_dummy;
    sensor->set_brightness = set_dummy;
    sensor->set_saturation = set_dummy;
    sensor->set_sharpness = set_dummy;
    sensor->set_denoise = set_dummy;
    sensor->set_gainceiling = set_gainceiling_dummy;
    sensor->set_quality = set_dummy;
    sensor->set_colorbar = set_colorbar;
    sensor->set_whitebal = set_dummy;
    sensor->set_gain_ctrl = set_dummy;
    sensor->set_exposure_ctrl = set_dummy;
    sensor->set_hmirror = set_hmirror;
    sensor->set_vflip = set_vflip;

    sensor->set_aec2 = set_dummy;
    sensor->set_awb_gain = set_dummy;
    sensor->set_agc_gain = set_dummy;
    sensor->set_aec_value = set_dummy;

    sensor->set_special_effect = set_dummy;
    sensor->set_wb_mode = set_dummy;
    sensor->set_ae_level = set_dummy;

    sensor->set_dcw = set_dummy;
    sensor->set_bpc = set_dummy;
    sensor->set_wpc = set_dummy;

    sensor->set_raw_gma = set_dummy;
    sensor->set_lenc = set_dummy;

    sensor->get_reg = get_reg;
    sensor->set_reg = set_reg;
    sensor->set_res_raw = NULL;
    sensor->set_pll = NULL;
    sensor->set_xclk = NULL;

    CAMERA_LOGD(TAG"GC032A Attached");
    return 0;
}


