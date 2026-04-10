/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * TC6036 camera sensor driver.
 */

#include "systick.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "tc6036: "
#include "sensor.h"
#include "tc6036.h"
#include "tc6036_regs.h"
#include "tc6036_settings.h"

#define H8(v) ((v) >> 8)
#define L8(v) ((v) & 0xff)

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
        ret = CAMERA_WRITE_REG8(slv_addr, reg, value);
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

static void print_regs(uint8_t slv_addr)
{
#ifdef DEBUG_PRINT_REG
    CAMERA_DELAY_MS(100);
    CAMERA_LOGI(TAG"REG list look ======================");

    CAMERA_LOGI(TAG"\npage 0 ===");
    write_reg(slv_addr, REG_PAGE_SELECT, 0x00);
    for (size_t i = 0x00; i <= 0x2b; i++) {
        CAMERA_LOGI(TAG"p0 reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }

    CAMERA_LOGI(TAG"\npage 1 ===");
    write_reg(slv_addr, REG_PAGE_SELECT, 0x01);
    for (size_t i = 0x08; i <= 0xcb; i++) {
        CAMERA_LOGI(TAG"p1 reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }

    CAMERA_LOGI(TAG"\npage 2 ===");
    write_reg(slv_addr, REG_PAGE_SELECT, 0x02);
    for (size_t i = 0x11; i <= 0xe0; i++) {
        CAMERA_LOGI(TAG"p2 reg[0x%02x] = 0x%02x", i, read_reg(slv_addr, i));
    }
#endif
}

static int reset(sensor_t *sensor)
{
    int ret;

    /* Software Reset: write reset sequence */
    ret = write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x01);
    if (ret) {
        CAMERA_LOGE(TAG"Page select FAILED!");
        return ret;
    }
    // ret = write_reg(sensor->slv_addr, 0xc4, 0x80);
    // if (ret) {
    //     CAMERA_LOGE(TAG"Software Reset FAILED!");
    //     return ret;
    // }
    // CAMERA_DELAY_MS(200);

    ret = write_reg(sensor->slv_addr, 0xc4, 0x00);
    if (ret) {
        CAMERA_LOGE(TAG"Software Reset release FAILED!");
        return ret;
    }
    CAMERA_DELAY_MS(50);

    ret = write_regs(sensor->slv_addr, tc6036_default_regs,
                     sizeof(tc6036_default_regs) / (sizeof(uint8_t) * 2));
    if (ret == 0) {
        CAMERA_LOGD(TAG"Camera defaults loaded");
        CAMERA_DELAY_MS(100);
    }

    return ret;
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret = 0;

    switch (pixformat) {
    case PIXFORMAT_RGB565:
        write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x02);
        ret = write_reg(sensor->slv_addr, P2_OUTPUT_FORMAT, 0x06); // RGB565
        break;

    case PIXFORMAT_YUV422:
        write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x02);
        ret = write_reg(sensor->slv_addr, P2_OUTPUT_FORMAT, 0x04); // YUV422
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
    write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x02);
    uint8_t reg_val = read_reg(sensor->slv_addr, P2_OUTPUT_FORMAT);

    switch (reg_val) {
    case 0x06:
        return PIXFORMAT_RGB565;
    case 0x04:
        return PIXFORMAT_YUV422;
    default:
        return PIXFORMAT_INVALID;
    }
}

static int set_window(sensor_t *sensor, int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    int ret = 0;
    uint16_t h_st, h_end, v_st, v_end;
    uint8_t reg_82, reg_85;

    /* Calculate start and end positions */
    h_st = x;
    h_end = x + w;
    v_st = y;
    v_end = y + h;

    /* Combine high bits: {end[9:8], st[9:8]} */
    reg_82 = ((H8(h_end) & 0x03) << 4) | (H8(h_st) & 0x03);
    reg_85 = ((H8(v_end) & 0x03) << 4) | (H8(v_st) & 0x03);

    /* Configure crop window via page 2 registers */
    ret |= write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x02);
    ret |= write_reg(sensor->slv_addr, P2_CROP_80, L8(h_st));   // r_h_st[7:0]
    ret |= write_reg(sensor->slv_addr, P2_CROP_81, L8(h_end));  // r_h_end[7:0]
    ret |= write_reg(sensor->slv_addr, P2_CROP_82, reg_82);     // {r_h_end[9:8], r_h_st[9:8]}
    ret |= write_reg(sensor->slv_addr, P2_CROP_83, L8(v_st));   // r_v_st[7:0]
    ret |= write_reg(sensor->slv_addr, P2_CROP_84, L8(v_end));  // r_v_end[7:0]
    ret |= write_reg(sensor->slv_addr, P2_CROP_85, reg_85);     // {r_v_end[9:8], r_v_st[9:8]}

    if (ret == 0) {
        CAMERA_LOGD(TAG"Set window to: (%d,%d) %ux%u", x, y, w, h);
    }

    print_regs(sensor->slv_addr);

    return ret;
}

static int get_window(sensor_t *sensor, uint16_t *w, uint16_t *h)
{
    int ret = 0;
    int reg_80, reg_81, reg_82, reg_83, reg_84, reg_85;
    uint16_t h_st, h_end, v_st, v_end;

    /* Select page 2 */
    ret = write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x02);
    if (ret < 0) {
        return ret;
    }

    /* Read horizontal window registers */
    reg_80 = read_reg(sensor->slv_addr, P2_CROP_80);  // r_h_st[7:0]
    reg_81 = read_reg(sensor->slv_addr, P2_CROP_81);  // r_h_end[7:0]
    reg_82 = read_reg(sensor->slv_addr, P2_CROP_82);  // {r_h_end[9:8], r_h_st[9:8]}

    /* Read vertical window registers */
    reg_83 = read_reg(sensor->slv_addr, P2_CROP_83);  // r_v_st[7:0]
    reg_84 = read_reg(sensor->slv_addr, P2_CROP_84);  // r_v_end[7:0]
    reg_85 = read_reg(sensor->slv_addr, P2_CROP_85);  // {r_v_end[9:8], r_v_st[9:8]}

    if (reg_80 < 0 || reg_81 < 0 || reg_82 < 0 ||
        reg_83 < 0 || reg_84 < 0 || reg_85 < 0) {
        return -1;
    }

    /* Calculate horizontal start and end (10-bit values) */
    h_st = ((reg_82 & 0x03) << 8) | reg_80;           // bit[1:0] of reg_82 as high bits
    h_end = ((reg_82 & 0x30) << 4) | reg_81;          // bit[5:4] of reg_82 as high bits

    /* Calculate vertical start and end (10-bit values) */
    v_st = ((reg_85 & 0x03) << 8) | reg_83;           // bit[1:0] of reg_85 as high bits
    v_end = ((reg_85 & 0x30) << 4) | reg_84;          // bit[5:4] of reg_85 as high bits

    /* Calculate width and height */
    *w = h_end - h_st;
    *h = v_end - v_st;

    return 0;
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.hmirror = enable;

    ret = write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_MIRROR_FLIP, 0, 0x01, enable != 0);

    if (ret == 0) {
        CAMERA_LOGD(TAG"Set h-mirror to: %d", enable);
    }
    return ret;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    int ret = 0;
    sensor->status.vflip = enable;

    ret = write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x00);
    ret |= set_reg_bits(sensor->slv_addr, P0_MIRROR_FLIP, 1, 0x01, enable != 0);

    if (ret == 0) {
        CAMERA_LOGD(TAG"Set v-flip to: %d", enable);
    }
    return ret;
}

static int set_colorbar(sensor_t *sensor, int enable)
{
    sensor->status.colorbar = enable;
    CAMERA_LOGD(TAG"Set colorbar to: %d", enable);
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
        /* Not supported */
    } else {
        ret = write_reg(sensor->slv_addr, reg, value);
    }
    return ret;
}

static int init_status(sensor_t *sensor)
{
    write_reg(sensor->slv_addr, REG_PAGE_SELECT, 0x00);
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

    print_regs(sensor->slv_addr);
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

int tc6036_detect(int slv_addr, sensor_id_t *id)
{
    if (TC6036_SCCB_ADDR == slv_addr) {
        write_reg(slv_addr, REG_PAGE_SELECT, 0x02);
        uint8_t MIDH = read_reg(slv_addr, REG_CHIP_ID_HIGH);
        uint8_t MIDL = read_reg(slv_addr, REG_CHIP_ID_LOW);
        uint16_t PID = (MIDH << 8) | MIDL;
        if (TC6036_PID == PID) {
            id->PID = PID;
            return PID;
        }
        CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
    }
    return 0;
}

int tc6036_init(sensor_t *sensor)
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
    sensor->get_window = get_window;
    sensor->get_pixformat = get_pixformat;

    sensor->set_raw_gma = set_dummy;
    sensor->set_lenc = set_dummy;

    sensor->get_reg = get_reg;
    sensor->set_reg = set_reg;
    sensor->set_res_raw = NULL;
    sensor->set_pll = NULL;
    sensor->set_xclk = NULL;

    CAMERA_LOGD(TAG"TC6036 Attached");
    return 0;
}
