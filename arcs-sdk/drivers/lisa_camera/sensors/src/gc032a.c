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
#include <stdio.h>

#define TAG  "gc032a: "
#include "sensor.h"
#include "gc032a.h"
#include "gc032a_regs.h"
#include "gc032a_settings.h"


#define H8(v) ((v)>>8)
#define L8(v) ((v)&0xff)

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

static int write_regs(uint8_t slv_addr, const uint16_t (*regs)[2])
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
    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0xf0);
    if (ret) {
        CAMERA_LOGE(TAG"Software Reset FAILED!");
        return ret;
    }
    CAMERA_DELAY_MS(100);

    ret = write_regs(sensor->slv_addr, gc032a_init_reg_tb);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Camera defaults loaded");
        CAMERA_DELAY_MS(100);
        write_reg(sensor->slv_addr, 0xfe, 0x00);
        set_reg_bits(sensor->slv_addr, 0xf7, 1, 0x01, 1); // PLL_mode1:div2en
        set_reg_bits(sensor->slv_addr, 0xf7, 7, 0x01, 1); // PLL_mode1:dvp mode
        set_reg_bits(sensor->slv_addr, 0xf8, 0, 0x3f, 8); //PLL_mode2 :divx4
        set_reg_bits(sensor->slv_addr, 0xfa, 4, 0x0f, 2); //vlk div mode :divide_by
    }

    return ret;
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret = 0;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        write_reg(sensor->slv_addr, 0xfe, 0x00);
        ret = set_reg_bits(sensor->slv_addr, 0x44, 0, 0x1f, 6);  //RGB565
        break;

    case PIXFORMAT_YUV422:
        write_reg(sensor->slv_addr, 0xfe, 0x00);
        ret = set_reg_bits(sensor->slv_addr, 0x44, 0, 0x1f, 3);
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

static pixformat_t get_pixformat(sensor_t *sensor)
{
    int reg_val = read_reg(sensor->slv_addr, 0x44);
    printf("reg_val: %d\n", reg_val);
    if (reg_val < 0) {
        return PIXFORMAT_INVALID;
    }

    switch (reg_val & 0x1F) {
    case 6:
        return PIXFORMAT_RGB565;
    case 3:
        return PIXFORMAT_YUV422;
    default:
        return PIXFORMAT_INVALID;
    }
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.hmirror = enable;
    ret = write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_CISCTL_MODE1, 0, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set h-mirror to: %d", enable);
    }
    return ret;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.vflip = enable;
    ret = write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_CISCTL_MODE1, 1, 0x01, enable);
    if (ret == 0) {
        CAMERA_LOGD(TAG"Set v-flip to: %d", enable);
    }
    return ret;
}

static int set_colorbar(sensor_t *sensor, int enable)
{
    int ret = 0;
    ret = write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_DEBUG_MODE2, 3, 0x01, enable);
    if (ret == 0) {
        sensor->status.colorbar = enable;
        CAMERA_LOGD(TAG"Set colorbar to: %d", enable);
    }
    return ret;
}

static int set_window(sensor_t *sensor, int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    int ret = 0;
    uint16_t row_s = (uint16_t)y;
    uint16_t col_s = (uint16_t)x;

    write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret |= write_reg(sensor->slv_addr, P0_ROW_START_HIGH, H8(row_s));
    ret |= write_reg(sensor->slv_addr, P0_ROW_START_LOW, L8(row_s));
    ret |= write_reg(sensor->slv_addr, P0_COLUMN_START_HIGH, H8(col_s));
    ret |= write_reg(sensor->slv_addr, P0_COLUMN_START_LOW, L8(col_s));
    ret |= write_reg(sensor->slv_addr, P0_WINDOW_HEIGHT_HIGH, H8(h + 8));
    ret |= write_reg(sensor->slv_addr, P0_WINDOW_HEIGHT_LOW, L8(h + 8));
    ret |= write_reg(sensor->slv_addr, P0_WINDOW_WIDTH_HIGH, H8(w + 8));
    ret |= write_reg(sensor->slv_addr, P0_WINDOW_WIDTH_LOW, L8(w + 8));

    ret |= write_reg(sensor->slv_addr, P0_WIN_MODE, 0x01);
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_HIGH, H8(h));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_LOW, L8(h));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_HIGH, H8(w));
    ret |= write_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_LOW, L8(w));

    if (ret == 0) {
        CAMERA_LOGD(TAG"Set window to: x=%d, y=%d, w=%u, h=%u", x, y, w, h);
    }
    return ret;
}

static int get_window(sensor_t *sensor, uint16_t *w, uint16_t *h)
{
    int ret = 0;
    uint8_t h_high, h_low, w_high, w_low;

    write_reg(sensor->slv_addr, 0xfe, 0x00);
    ret = read_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_HIGH);
    if (ret < 0) return ret;
    h_high = (uint8_t)ret;

    ret = read_reg(sensor->slv_addr, P0_OUT_WIN_HEIGHT_LOW);
    if (ret < 0) return ret;
    h_low = (uint8_t)ret;

    ret = read_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_HIGH);
    if (ret < 0) return ret;
    w_high = (uint8_t)ret;

    ret = read_reg(sensor->slv_addr, P0_OUT_WIN_WIDTH_LOW);
    if (ret < 0) return ret;
    w_low = (uint8_t)ret;

    *h = (h_high << 8) | h_low;
    *w = (w_high << 8) | w_low;

    // CAMERA_LOGD(TAG"Get window: w=%u, h=%u", *w, *h);
    printf("Get window: w=%u, h=%u\n", *w, *h);
    return 0;
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
    write_reg(sensor->slv_addr, 0xfe, 0x00);
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
    sensor->status.hmirror = check_reg_mask(sensor->slv_addr, P0_CISCTL_MODE1, 0x01);
    sensor->status.vflip = check_reg_mask(sensor->slv_addr, P0_CISCTL_MODE1, 0x02);
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

int gc032a_detect(int slv_addr, sensor_id_t *id)
{
    if (GC032A_SCCB_ADDR == slv_addr) {
        uint8_t MIDL = CAMERA_READ_REG8(slv_addr, SENSOR_ID_LOW);
        uint8_t MIDH = CAMERA_READ_REG8(slv_addr, SENSOR_ID_HIGH);
        uint16_t PID = MIDH << 8 | MIDL;
        if (GC032A_PID == PID) {
            id->PID = PID;
            return PID;
        }
        CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
    }
    return 0;
}

int gc032a_init(sensor_t *sensor)
{
    sensor->init_status = init_status;
    sensor->reset = reset;
    sensor->set_pixformat = set_pixformat;
    // sensor->set_framesize = set_framesize;
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
    sensor->set_window = set_window;

    sensor->get_window = get_window;
    sensor->get_pixformat = get_pixformat;

    sensor->get_reg = get_reg;
    sensor->set_reg = set_reg;
    sensor->set_res_raw = NULL;
    sensor->set_pll = NULL;
    sensor->set_xclk = NULL;

    CAMERA_LOGD(TAG"GC032A Attached");
    return 0;
}
