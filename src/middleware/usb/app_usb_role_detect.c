/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "usb-role-detect"

#include "FreeRTOS.h"
#include "app_usb_role_detect.h"

#include "board.h"
#include "lisa_device.h"
#include "lisa_i2c.h"
#include "lisa_log.h"
#include "task.h"

#define USB_ROLE_STATE_MACHINE_STARTUP_DELAY_MS 200U

static lisa_device_t *s_role_detect_i2c_dev;
static bool s_role_detect_initialized;
static bool s_role_detect_init_error_logged;
static app_usb_role_t s_last_reported_role;

static int app_usb_role_detect_read_register(uint8_t reg, uint8_t *value);
static int app_usb_role_detect_write_register(uint8_t reg, uint8_t value);

static int app_usb_role_detect_validate_controller(void)
{
    uint8_t device_id;
    uint8_t device_type;
    uint8_t portrole;
    uint8_t control;
    uint8_t control1;
    int ret;

    ret = app_usb_role_detect_read_register(USB_ROLE_DEVICE_ID_REGISTER, &device_id);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_DEVICE_TYPE_REGISTER, &device_type);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    if (device_id != USB_ROLE_DEVICE_ID_EXPECTED ||
        device_type != USB_ROLE_DEVICE_TYPE_EXPECTED) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "controller identity invalid: DEVICE_ID=0x%02x DEVICE_TYPE=0x%02x expected=0x%02x/0x%02x",
                      device_id, device_type,
                      USB_ROLE_DEVICE_ID_EXPECTED, USB_ROLE_DEVICE_TYPE_EXPECTED);
            s_role_detect_init_error_logged = true;
        }
        return LISA_DEVICE_ERR_IO;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_PORTROLE_REGISTER, &portrole);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    /* Audio accessory is obviously not needed */
    portrole &= ~USB_ROLE_PORTROLE_AUDIOACC;

#if defined(CONFIG_APP_USB_ROLE_DRP)
    portrole &= ~(USB_ROLE_PORTROLE_MASK | USB_ROLE_PORTROLE_TRY_MASK);
    portrole |= USB_ROLE_PORTROLE_DRP | USB_ROLE_PORTROLE_TRY_SINK;
#elif defined(CONFIG_APP_USB_ROLE_HOST_ONLY)
    portrole &= ~(USB_ROLE_PORTROLE_MASK | USB_ROLE_PORTROLE_TRY_MASK);
    portrole |= USB_ROLE_PORTROLE_SOURCE_ONLY | USB_ROLE_PORTROLE_TRY_SOURCE;
#elif defined(CONFIG_APP_USB_ROLE_DEVICE_ONLY)
    portrole &= ~(USB_ROLE_PORTROLE_MASK | USB_ROLE_PORTROLE_TRY_MASK);
    portrole |= USB_ROLE_PORTROLE_SINK_ONLY | USB_ROLE_PORTROLE_TRY_SINK;
#endif

    ret = app_usb_role_detect_write_register(USB_ROLE_PORTROLE_REGISTER, portrole);
    if (ret != LISA_DEVICE_OK) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "PORTROLE DRP/Try.SNK write failed: %d", ret);
            s_role_detect_init_error_logged = true;
        }
        return ret;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_CONTROL_REGISTER, &control);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    if (!s_role_detect_init_error_logged) {
        LISA_LOGI(TAG, "raw CONTROL=0x%02x rp_bits=0x%02x",
                  control, control & USB_ROLE_CONTROL_RP_MASK);
    }

    control = (control & ~USB_ROLE_CONTROL_RP_MASK) |
              USB_ROLE_CONTROL_RP_DEFAULT;
    ret = app_usb_role_detect_write_register(USB_ROLE_CONTROL_REGISTER, control);
    if (ret != LISA_DEVICE_OK) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "CONTROL Default Rp write failed: %d", ret);
            s_role_detect_init_error_logged = true;
        }
        return ret;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_CONTROL_REGISTER, &control);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    if (!s_role_detect_init_error_logged) {
        LISA_LOGI(TAG, "CONTROL after Default Rp=0x%02x rp_bits=0x%02x",
                  control, control & USB_ROLE_CONTROL_RP_MASK);
    }

    if ((control & USB_ROLE_CONTROL_RP_MASK) != USB_ROLE_CONTROL_RP_DEFAULT) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "CONTROL Default Rp verification failed");
            s_role_detect_init_error_logged = true;
        }
        return LISA_DEVICE_ERR_IO;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_CONTROL1_REGISTER, &control1);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    if (!s_role_detect_init_error_logged) {
        LISA_LOGI(TAG, "raw DEVICE_ID=0x%02x DEVICE_TYPE=0x%02x CONTROL1=0x%02x control1_bit3=%u",
                  device_id, device_type, control1,
                  (control1 & USB_ROLE_CONTROL1_REQUIRED_MASK) != 0U);
    }

    control1 |= USB_ROLE_CONTROL1_REQUIRED_MASK;
    ret = app_usb_role_detect_write_register(USB_ROLE_CONTROL1_REGISTER, control1);
    if (ret != LISA_DEVICE_OK) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "CONTROL1 state machine enable write failed: %d", ret);
            s_role_detect_init_error_logged = true;
        }
        return ret;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_CONTROL1_REGISTER, &control1);
    if (ret != LISA_DEVICE_OK) {
        goto read_failed;
    }

    if (!s_role_detect_init_error_logged) {
        LISA_LOGI(TAG, "CONTROL1 after enable=0x%02x control1_bit3=%u",
                  control1, (control1 & USB_ROLE_CONTROL1_REQUIRED_MASK) != 0U);
    }

    if ((control1 & USB_ROLE_CONTROL1_REQUIRED_MASK) == 0U) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "CONTROL1 state machine enable verification failed");
            s_role_detect_init_error_logged = true;
        }
        return LISA_DEVICE_ERR_IO;
    }

    vTaskDelay(pdMS_TO_TICKS(USB_ROLE_STATE_MACHINE_STARTUP_DELAY_MS));
    return LISA_DEVICE_OK;

read_failed:
    if (!s_role_detect_init_error_logged) {
        LISA_LOGE(TAG, "controller register read failed: %d", ret);
        s_role_detect_init_error_logged = true;
    }
    return ret;
}

static const char *app_usb_portrole_name(uint8_t portrole)
{
    switch (portrole & USB_ROLE_PORTROLE_MASK) {
    case USB_ROLE_PORTROLE_SOURCE_ONLY:
        return "source-only";
    case USB_ROLE_PORTROLE_SINK_ONLY:
        return "sink-only";
    case USB_ROLE_PORTROLE_DRP:
        return "drp";
    default:
        return "unknown";
    }
}


int app_usb_role_detect_init(void)
{
    int ret;

    if (s_role_detect_initialized) {
        return 0;
    }

    s_role_detect_i2c_dev = lisa_device_get(USB_ROLE_I2C_DEVICE_NAME);
    if (!lisa_device_ready(s_role_detect_i2c_dev)) {
        if (!s_role_detect_init_error_logged) {
            LISA_LOGE(TAG, "I2C device %s is not ready", USB_ROLE_I2C_DEVICE_NAME);
            s_role_detect_init_error_logged = true;
        }
        return LISA_DEVICE_ERR_NOT_READY;
    }

    ret = app_usb_role_detect_validate_controller();
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    s_role_detect_initialized = true;
    s_role_detect_init_error_logged = false;
    LISA_LOGI(TAG, "initialized: I2C=%s addr=0x%02x status_reg=0x%02x type_reg=0x%02x",
              USB_ROLE_I2C_DEVICE_NAME, USB_ROLE_I2C_ADDRESS,
              USB_ROLE_STATUS_REGISTER, USB_ROLE_TYPE_REGISTER);
    return 0;
}

static int app_usb_role_detect_read_register(uint8_t reg, uint8_t *value)
{
    lisa_i2c_msg_t msgs[2] = {
        {
            .addr = USB_ROLE_I2C_ADDRESS,
            .flags = LISA_I2C_FLAG_NO_STOP,
            .len = 1,
            .buf = &reg,
        },
        {
            .addr = USB_ROLE_I2C_ADDRESS,
            .flags = LISA_I2C_FLAG_READ,
            .len = 1,
            .buf = value,
        },
    };

    return lisa_i2c_transfer(s_role_detect_i2c_dev, msgs, 2);
}

static int app_usb_role_detect_write_register(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };

    return lisa_i2c_write(s_role_detect_i2c_dev, USB_ROLE_I2C_ADDRESS,
                          data, sizeof(data));
}

int app_usb_role_detect_read(app_usb_role_t *role)
{
    uint8_t type;
    bool state_changed;
    int ret;

    if (role == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    *role = APP_USB_ROLE_UNKNOWN;

    ret = app_usb_role_detect_init();
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = app_usb_role_detect_read_register(USB_ROLE_TYPE_REGISTER, &type);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    if (type & USB_ROLE_TYPE_SOURCE_MASK) {
        *role = APP_USB_ROLE_HOST;
    } else {
        *role = APP_USB_ROLE_DEVICE;
    }

    state_changed = *role != s_last_reported_role;
    if (state_changed) {
        LISA_LOGI(TAG, "raw TYPE=0x%02x source=%u sink=%u",
                  type,
                  (type & USB_ROLE_TYPE_SOURCE_MASK) != 0U,
                  (type & USB_ROLE_TYPE_SINK_MASK) != 0U);
    }

    s_last_reported_role = *role;

    if (*role == APP_USB_ROLE_HOST || *role == APP_USB_ROLE_DEVICE) {
        return 1;
    }

    return LISA_DEVICE_ERR_IO;
}
