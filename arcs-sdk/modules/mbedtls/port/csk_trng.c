#include <stdint.h>
#include <string.h>

#include <mbedtls/entropy.h>

#include "ClockManager.h"
// #include "trng_reg.h"
#include "Driver_TRNG.h"
#include "log_print.h"

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t xSemaphore;

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
			  size_t *olen)
{
    uint32_t trng_data = 0;
    uint16_t copy_len = 0;
    uint16_t request_len = len > 0xFFFF ? 0xFFFF : len;

    static bool init_flag = false;

    (void)(data);

	if (output == NULL || olen == NULL || len == 0) {
		return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
	}

    if (!init_flag) {
        __HAL_CRM_TRNG_CLK_ENABLE();

        //initialize
        HAL_TRNG_Initialize(TRNG());
        HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
        HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
        HAL_TRNG_InterruptDisable(TRNG());

        xSemaphore = xSemaphoreCreateMutex();
        if (xSemaphore == NULL) {
            CLOGE("mbedtls_hardware_poll: xSemaphoreCreateMutex failed");
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }

        init_flag = true;
    }

    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    for(uint32_t cnt=0; cnt<request_len; )
    {
    	HAL_TRNG_Enable(TRNG());
    	while(HAL_TRNG_GetDataReady(TRNG()) == 0);
    	trng_data = HAL_TRNG_GetData(TRNG());

        copy_len = ((cnt + 4) > request_len) ? request_len - cnt : 4;
        memcpy(output + cnt, (uint8_t *)&trng_data, copy_len);

        cnt += copy_len;
    }

    *olen = request_len;
    xSemaphoreGive(xSemaphore);

    // HAL_TRNG_Uninitialize(TRNG());
    // HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    return 0;
}
