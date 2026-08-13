/**************************************************************************//**
 * @file     main.c
 * Copyright (C) 2018 ChipSky Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "venusa_ap.h"
#include "log_print.h"
#include <string.h>
#include "spiflash.h"
#include "clock_config.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "ClockManager.h"

#ifdef EXT_ADDR4_MODE
// 16M
//#define FLASH_TEST_OFFSET    (0x100000 * 16)
#else
// 0M
#define FLASH_ADDR_BASE         (0x30000000)
#define FLASHDL_BASE            (0x44300000)
#define FLASH_TEST_START     	(0x100000)
#define FLASH_TEST_OFFSET    	(0x100000 * 0)
#define CMN_FLASHC_BASE			(0x44200000)
#define FLASH_TEST_LEN          (1024 * 1024) 		// 1MB
#define FLASH_TEST_SECTOR_SIZE  (1024 * 4)
#endif

__attribute__ ((section (".ramcode")))
void flash_test_erase_write_read()
{
	CLOG("External Flash Test - Erase->Write->Read: 0x%08X - 0x%08X\n", (FLASH_TEST_START + FLASH_ADDR_BASE), (FLASH_TEST_START + FLASH_TEST_LEN + FLASH_ADDR_BASE - 1));

	int i;
	int ret;

	FLASH_DEV dev = {
		.base_addr = CMN_FLASHC_BASE,
		.d_width = 4,
//		.sclk_div = 0,    //divider is 2
		.sclk_div = 0xFF, //divider is 1
//		.sclk_div = 1,    //divider is 4
		.run_mod = RUN_WITH_INT,//RUN_WITH_INT//RUN_WITHOUT_INT
		.timeout = 2000000,
		.addr_bytes = 3,
		.addr_auto = 0,
	};

#if IC_BOARD == 0
	if(dev.sclk_div == 0xFF)
		CLOG("Run at %dHz\n", BOARD_BOOTCLOCKRUN_RC024M_CLK);
	else
		CLOG("Run at %dHz\n", BOARD_BOOTCLOCKRUN_RC024M_CLK / (dev.sclk_div + 1) / 2);
#else
	if(dev.sclk_div == 0xFF)
		CLOG("Run at %dHz\n", CRM_GetFlashFreq());
	else
		CLOG("Run at %dHz\n", CRM_GetFlashFreq() / (dev.sclk_div + 1) / 2);
#endif

	uint32_t dat = inw(FLASHDL_BASE);
	dat &= ~(1UL << 17);
	outw(FLASHDL_BASE, dat);  // disable delay line

	if(flash_init(&dev, 0, 0) != 0)
		CLOGD("FLash Init failed");

	static uint8_t buf[FLASH_TEST_SECTOR_SIZE * 2], buf2[FLASH_TEST_SECTOR_SIZE];

	int test_times = 0;

	while(1)
	{
		for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i += 4)
		{
			*(int*)(&buf[i]) = rand();
		}
		memcpy(&buf[FLASH_TEST_SECTOR_SIZE], buf, FLASH_TEST_SECTOR_SIZE);

		uint8_t *pOpBuf = buf;

		ret = flash_write_protection_set(&dev, false);
		for(uint32_t addr_curr = FLASH_TEST_START; addr_curr < (FLASH_TEST_START + FLASH_TEST_LEN); )
		{
			ret = flash_erase(&dev, addr_curr, (FLASH_TEST_SECTOR_SIZE * 16));
			if(ret) {
				CLOG("Can't erase 0x%08x", addr_curr);
			}

			for(int j = 0; j < 16; j++)
			{
				ret = flash_write(&dev, addr_curr, pOpBuf, FLASH_TEST_SECTOR_SIZE);
				if(ret) {
					CLOG("Can't write 0x%08x", addr_curr);
				}
				addr_curr += FLASH_TEST_SECTOR_SIZE;
			}
			pOpBuf += 4;
			if(pOpBuf > &buf[FLASH_TEST_SECTOR_SIZE])
			{
				pOpBuf = buf;
			}
		}
		ret = flash_write_protection_set(&dev, true);

		for(int loop_rd = 0; loop_rd < 10; loop_rd ++)
		{
			uint8_t sector_rd = 0;
			pOpBuf = buf;

			for(uint32_t addr_curr = FLASH_TEST_START; addr_curr < (FLASH_TEST_START + FLASH_TEST_LEN); addr_curr += FLASH_TEST_SECTOR_SIZE)
			{
				ret = flash_read(&dev, addr_curr, buf2, FLASH_TEST_SECTOR_SIZE);
				if(ret) {
					CLOG("Can't read 0x%08x", addr_curr);
				}
				int read_err = 0;
				for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i++)
				{
					if(pOpBuf[i] != buf2[i])
					{
						if(0 == read_err)
						{
							CLOG("0x%08x differs to read value 0x%08x at 0x%08x in API mode...", pOpBuf[i], buf2[i], (addr_curr + i + FLASH_ADDR_BASE));
						}
						read_err = 1; //avoid multiple print
					}
				}
				sector_rd++;
				if((sector_rd % 16) == 0)
				{
					sector_rd = 0;
					pOpBuf += 4;
				}
			}

			for(uint32_t addr_curr = FLASH_TEST_START; addr_curr < (FLASH_TEST_START + FLASH_TEST_LEN); addr_curr++)
			{
				if(buf[((addr_curr - FLASH_TEST_START) % FLASH_TEST_SECTOR_SIZE) + ((addr_curr - FLASH_TEST_START) / ( FLASH_TEST_SECTOR_SIZE * 16) * 4)] != *((uint8_t *)(addr_curr + FLASH_ADDR_BASE)))
				{
					CLOG("0x%02x differs to read value 0x%02x at 0x%08x in XIP mode",
							buf[(addr_curr - FLASH_TEST_START) % FLASH_TEST_SECTOR_SIZE],
							*((uint8_t *)(addr_curr + FLASH_ADDR_BASE)),
							(addr_curr + FLASH_ADDR_BASE));
				}
			}
		}
		test_times++;
		CLOG("Test %d", test_times);
	}
}


int main(void)
{
	logInit(0, 115200);
	flash_test_erase_write_read();
	while(1);
    return 0;
}
