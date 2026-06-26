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

#include "systick.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "gc0328: "
#include "sensor.h"
#include "gc0328.h"
#include "gc0328_regs.h"
#include "gc0328_settings.h"

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
    SysTick_Delay_Ms(60);
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
    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0xf0);
    if (ret) {
        CAMERA_LOGE(TAG"Software Reset FAILED!");
        return ret;
    }
    CAMERA_DELAY_MS(100);

    ret = write_regs(sensor->slv_addr, gc0328_default_regs, sizeof(gc0328_default_regs)/(sizeof(uint8_t) * 2));
    if (ret == 0) {
        CAMERA_LOGD(TAG"Camera defaults loaded");
        CAMERA_DELAY_MS(100);
        write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
    }

    return ret;
}


static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret = 0;

    switch (pixformat)
    {
        case PIXFORMAT_RGB565:
            write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
            ret = set_reg_bits(sensor->slv_addr, 0x44, 0, 0x1f, 0x06);  // 0x06:RGB565
            ret = set_reg_bits(sensor->slv_addr, 0x49, 0, 0x20, 0x20);  // little endian
            break;

        case PIXFORMAT_YUV422:
            write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
            ret = set_reg_bits(sensor->slv_addr, 0x44, 0, 0x1f, 0x02); // 0x00:CbYCrY 0x01:CrYCbY 0x02:YCbYCr 0x03:YCrYCb
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

/* ---- set_window: crop/读出窗口 (0x50 + 0x51-0x58) ------------------------- */
static int set_window(sensor_t *sensor, int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    int ret = 0;
    ret |= write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
    ret |= write_reg(sensor->slv_addr, P0_WIN_MODE, 0x01);
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_Y1_HIGH, H8((uint16_t)y));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_Y1_LOW,  L8((uint16_t)y));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_X1_HIGH, H8((uint16_t)x));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_X1_LOW,  L8((uint16_t)x));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_HIGH, H8(h));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_LOW,  L8(h));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_HIGH,  H8(w));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_LOW,   L8(w));
    if (ret == 0) {
        CAMERA_LOGD(TAG"crop window %ux%u@(%d,%d)", w, h, x, y);
    }
    return ret;
}

/* ---- set_subsample: 跳采比例 (0x59) + 使能 (0x5A) ------------------------- */

static int set_subsample(sensor_t *sensor, uint8_t row_ratio, uint8_t col_ratio)
{
    if (row_ratio < 1 || row_ratio > 7) row_ratio = 1;
    if (col_ratio < 1 || col_ratio > 7) col_ratio = 1;

    int ret = 0;
    ret |= write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
    /* 0x59: [7:4]=row ratio, [3:0]=col ratio */
    ret |= write_reg(sensor->slv_addr, P0_SUBSAMPLE_RATIO, (row_ratio << 4) | col_ratio);
    /* ROW_EN | COL_EN: 行列跳采使能
     * NEIGHBOR_AVG:     邻域平均, 减少跳采锯齿
     * EXTEND_PCLK:      扩展 PCLK 脉宽, 保证 DVP 采样稳定 */
    ret |= write_reg(sensor->slv_addr, P0_SUBSAMPLE_MODE,
                     P0_SUBSAMPLE_MODE_ROW_EN
                     | P0_SUBSAMPLE_MODE_COL_EN
                     | P0_SUBSAMPLE_MODE_NEIGHBOR_AVG
                     | P0_SUBSAMPLE_MODE_EXTEND_PCLK);
    if (ret == 0) {
        CAMERA_LOGD(TAG"subsample row=1/%u col=1/%u", row_ratio, col_ratio);
    }
    return ret;
}


static int set_hmirror(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.hmirror = enable;
    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_MIRROR_FLIP, 0, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set h-mirror to: %d", enable);
    }
    return ret;
}


static int set_vflip(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.vflip = enable;
    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_MIRROR_FLIP, 1, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set v-flip to: %d", enable);
    }
    return ret;
}


static int get_window(sensor_t *sensor, uint16_t *w, uint16_t *h)
{
    int ret = 0;
    uint8_t h_high, h_low, w_high, w_low;
    uint8_t ratio_reg;

    /* 读取 crop 窗口尺寸 (0x55-0x58) 和跳采比例 (0x59)，
     * 返回跳采后的有效输出分辨率 = crop / ratio */
    ret |= write_reg(sensor->slv_addr, RESET_RELATED, 0x00);

    h_high = read_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_HIGH);
    h_low  = read_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_LOW);
    w_high = read_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_HIGH);
    w_low  = read_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_LOW);
    ratio_reg = read_reg(sensor->slv_addr, P0_SUBSAMPLE_RATIO);

    uint16_t crop_w = (w_high << 8) | w_low;
    uint16_t crop_h = (h_high << 8) | h_low;

    /* 0x59[7:4]=row, [3:0]=col */
    uint8_t row_ratio = (ratio_reg >> 4) & 0x0F;
    uint8_t col_ratio = ratio_reg & 0x0F;
    if (row_ratio == 0) row_ratio = 1;
    if (col_ratio == 0) col_ratio = 1;

    *w = crop_w / col_ratio;
    *h = crop_h / row_ratio;

    return ret;
}

static int set_colorbar(sensor_t *sensor, int enable)
{
    int ret = 0;

    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_DEBUG_MODE2, 0, 0x01, enable);

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
    write_reg(sensor->slv_addr, RESET_RELATED, 0x00);
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
    sensor->status.hmirror = check_reg_mask(sensor->slv_addr, P0_MIRROR_FLIP, 0x01);
    sensor->status.vflip = check_reg_mask(sensor->slv_addr, P0_MIRROR_FLIP, 0x02);
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


static pixformat_t get_pixformat(sensor_t *sensor)
{
    uint8_t reg_val = read_reg(sensor->slv_addr, 0x44) & 0x1f;

    switch (reg_val) {
        case 0x06:
            return PIXFORMAT_RGB565;
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            return PIXFORMAT_YUV422;
        default:
            return PIXFORMAT_INVALID;
    }
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


int gc0328_detect(int slv_addr, sensor_id_t *id)
{
    if (GC0328_SCCB_ADDR == slv_addr) {
        //write_reg(slv_addr, RESET_RELATED, 0x00);
        uint8_t PID = read_reg(slv_addr, REG_CHIP_ID);
        if (GC0328_PID == PID) {
            id->PID = PID;
            return PID;
        }
        CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
    }
    return 0;
}


int gc0328_init(sensor_t *sensor)
{
    sensor->init_status = init_status;
    sensor->reset = reset;
    sensor->set_pixformat = set_pixformat;
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
    sensor->set_window = set_window;
    sensor->set_subsample = set_subsample;

    sensor->get_window = get_window;
    sensor->get_pixformat = get_pixformat;
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
