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
#include "bf20a6.h"
#include "bf20a6_regs.h"
#include "bf20a6_settings.h"


#define TAG  "bf20a6: "

#define H8(v) ((v)>>8)
#define L8(v) ((v)&0xff)

//#define REG_DEBUG_ON

static int read_reg(uint8_t slv_addr, const uint16_t reg)
{
    int ret = CAMERA_READ_REG8(slv_addr, reg);
    // CAMERA_LOGI(TAG"READ Register 0x%02x VALUE: 0x%02x", reg, ret);
#ifdef REG_DEBUG_ON
    if (ret < 0) {
        CAMERA_LOGE(TAG"READ REG 0x%04x FAILED: %d", reg, ret);
    }
#endif
    return ret;
}

static int write_reg(uint8_t slv_addr, const uint16_t reg, uint8_t value)
{
    int ret = CAMERA_WRITE_REG8(slv_addr, reg, value);
#ifdef REG_DEBUG_ON
    if (ret < 0) {
        CAMERA_LOGE(TAG"WRITE REG 0x%04x FAILED: %d", reg, ret);
    }
#endif
    return ret;
}

#ifdef DEBUG_PRINT_REG
static int check_reg_mask(uint8_t slv_addr, uint16_t reg, uint8_t mask)
{
    return (read_reg(slv_addr, reg) & mask) == mask;
}

static void print_regs(uint8_t slv_addr)
{
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
}

static int read_regs(uint8_t slv_addr, const uint16_t(*regs)[2])
{
    int i = 0, ret = 0;
    while (regs[i][0] != REGLIST_TAIL) {
        if (regs[i][0] == REG_DLY) {
            CAMERA_DELAY_MS(regs[i][1]);
        } else {
            ret = read_reg(slv_addr, regs[i][0]);
        }
        i++;
    }
    return ret;
}
#endif

static int set_reg_bits(sensor_t *sensor, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;

    ret = CAMERA_READ_REG8(sensor->slv_addr, reg);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, reg & 0xFF, value);
    return ret;
}

static int write_regs(uint8_t slv_addr, const uint16_t(*regs)[2])
{
    int i = 0, ret = 0;
    while (!ret && regs[i][0] != REGLIST_TAIL) {
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
    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0x01);
    if (ret) {
        CAMERA_LOGE(TAG"Software Reset FAILED!");
        return ret;
    }
    CAMERA_DELAY_MS(100);

    ret = write_regs(sensor->slv_addr, bf20a6_default_init_regs);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Camera defaults loaded");
        CAMERA_DELAY_MS(100);
    }

    // int test_value = read_regs(sensor->slv_addr, bf20a6_default_init_regs);

    return ret;
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret = 0;
    switch (pixformat) {
    case PIXFORMAT_YUV422:
        set_reg_bits(sensor, 0x12, 0, 1, 0);
        break;
    case PIXFORMAT_RAW:
        set_reg_bits(sensor, 0x12, 0, 1, 0x1);
        break;
    case PIXFORMAT_GRAYSCALE:
        write_reg(sensor->slv_addr, 0x12, 0x23);
        write_reg(sensor->slv_addr, 0x3a, 0x00);
        write_reg(sensor->slv_addr, 0xe1, 0x92);
        write_reg(sensor->slv_addr, 0xe3, 0x02);
        break;
    default:
        CAMERA_LOGW(TAG"set_pix unsupport format");
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
    int ret = 0;
    if (framesize > FRAMESIZE_VGA) {
        return -1;
    }
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;

    sensor->status.framesize = framesize;

    // Write MSBs
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x17, 0);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x18, w >> 2);

    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x19, 0);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x1a, h >> 2);

    // Write LSBs
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x1b, 0);

    if ((w <= 320) && (h <= 240))     {
        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x17, (80 - w / 4));
        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x18, (80 + w / 4));

        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x19, (60 - h / 4));

        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x1a, (60 + h / 4));

    } else if ((w <= 640) && (h <= 480))     {
        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x17, (80 - w / 8));
        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x18, (80 + w / 8));

        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x19, (60 - h / 8));

        ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x1a, (60 + h / 8));
    }

    // Delay
    CAMERA_DELAY_MS(30);

    return ret;
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.hmirror = enable;
    //ret = write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret |= set_reg_bits(sensor, 0x4a, 3, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set h-mirror to: %d", enable);
    }
    return ret;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.vflip = enable;
    //ret = write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret |= set_reg_bits(sensor, 0x4a, 2, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set v-flip to: %d", enable);
    }
    return ret;
}

static int set_colorbar(sensor_t *sensor, int value)
{
    int ret = 0;
    ret = write_reg(sensor->slv_addr, 0xb6, value);
    if (ret == 0) {
        sensor->status.colorbar = value;
        CAMERA_LOGD(TAG"Set colorbar to: %d", value);
    }
    return ret;
}

static int set_sharpness(sensor_t *sensor, int level)
{
    int ret = 0;
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0x70, level);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set sharpness to: %d", level);
        sensor->status.sharpness = level;
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
    // write_reg(sensor->slv_addr, 0xfe, 0x00);
    sensor->status.brightness = CAMERA_READ_REG8(sensor->slv_addr, 0x6f);
    sensor->status.contrast = CAMERA_READ_REG8(sensor->slv_addr, 0xd6);
    sensor->status.saturation = 0;
    sensor->status.sharpness = CAMERA_READ_REG8(sensor->slv_addr, 0x70);
    sensor->status.denoise = 0;
    sensor->status.ae_level = 0;
    sensor->status.gainceiling = CAMERA_READ_REG8(sensor->slv_addr, 0x13);
    sensor->status.awb = 0;
    sensor->status.dcw = 0;
    sensor->status.agc = 0;
    sensor->status.aec = 0;
    sensor->status.hmirror = 0;// check_reg_mask(sensor->slv_addr, P0_CISCTL_MODE1, 0x01);
    sensor->status.vflip = 0;// check_reg_mask(sensor->slv_addr, P0_CISCTL_MODE1, 0x02);
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
    CAMERA_LOGW(TAG"dummy Unsupported");
    return -1;
}
static int set_gainceiling_dummy(sensor_t *sensor, gainceiling_t val)
{
    CAMERA_LOGW(TAG"gainceiling Unsupported");
    return -1;
}

int bf20a6_detect(int slv_addr, sensor_id_t *id)
{
    if (BF20A6_SCCB_ADDR == slv_addr) {
        uint8_t MIDL = CAMERA_READ_REG8(slv_addr, SENSOR_ID_LOW);
        uint8_t MIDH = CAMERA_READ_REG8(slv_addr, SENSOR_ID_HIGH);
        uint16_t PID = MIDH << 8 | MIDL;
        if (BF20A6_PID == PID) {
            id->PID = PID;
            return PID;
        } else {
            CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
        }
    }
    return 0;
}

int bf20a6_init(sensor_t *sensor)
{
    sensor->init_status = init_status;
    sensor->reset = reset;
    sensor->set_pixformat = set_pixformat;
    sensor->set_framesize = set_framesize;
    sensor->set_contrast = set_dummy;
    sensor->set_brightness = set_dummy;
    sensor->set_saturation = set_dummy;
    sensor->set_sharpness = set_sharpness;
    sensor->set_denoise = set_dummy;
    sensor->set_gainceiling = set_gainceiling_dummy;
    sensor->set_quality = set_dummy;
    sensor->set_colorbar = set_colorbar;
    sensor->set_whitebal = set_dummy;
    sensor->set_gain_ctrl = set_dummy;
    sensor->set_exposure_ctrl = set_dummy;
    sensor->set_hmirror = set_hmirror; // set_hmirror;
    sensor->set_vflip = set_vflip; // set_vflip;

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

    CAMERA_LOGD(TAG"BF20A6 Attached");
    return 0;
}
