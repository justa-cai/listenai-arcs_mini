/**************************************************************************//**
 * @file     main.c
 * Copyright (C) 2018 ChipSky Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "arcs_ap.h"
#include "log_print.h"
#include <string.h>
#include "nvs.h"
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
#define FLASHDL_BASE            (0x47700000)
#define FLASH_TEST_START     	(0x100000)
#define FLASH_TEST_OFFSET    	(0x100000 * 0)
#define FLASH_TEST_LEN          (1024 * 1024) 		// 1MB
#define FLASH_TEST_SECTOR_SIZE  (1024 * 4)
#endif

static void* GPIOA_Handler = NULL;

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
		CLOG("Run at %dHz\n", IC_BOARD_FPGA_FIX_FREQ);
	else
		CLOG("Run at %dHz\n", IC_BOARD_FPGA_FIX_FREQ / (dev.sclk_div + 1) / 2);
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


#define FLASH_ADDR_BASE         (0x30000000)
#define FLASH_TEST_OFFSET    	(0x100000 * 0)
#define FLASH_TEST_SECTOR_SIZE  (1024 * 4)
#define DUAL_FLASH_TEST_LEN     (1024 * 64) 		// 64K
#define DUAL_FLASH_TEST_START   (0x0)

#define FLASH0_TEST_START		(0)
#define FLASH0_TEST_LEN			(1024 * 32)			// 32K
#define FLASH1_TEST_START		(FLASH0_TEST_LEN)   // Start immediately after Flash0
#define FLASH1_TEST_LEN			(1024 * 32)			// 32K


__attribute__ ((section (".ramcode")))
void dual_flash_test() {
	CLOG("Dual Flash Test! Test Length: 0x%08X, Flash0: 0x%08X - 0x%08X, Flash1: 0x%08X - 0x%08X",
			DUAL_FLASH_TEST_LEN,
			FLASH0_TEST_START + FLASH_ADDR_BASE,
			FLASH0_TEST_START + FLASH0_TEST_LEN - 1 + FLASH_ADDR_BASE,
			FLASH1_TEST_START + FLASH_ADDR_BASE,
			FLASH1_TEST_START + FLASH1_TEST_LEN - 1 + FLASH_ADDR_BASE);

	int i;
	int ret;

    DisableDCache();
    __RWMB();
    __FENCE_I();

	//Config gpioA_16 CS1
	GPIOA_Handler = GPIOA();
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER1); // CSK_IOMUX_FUNC_ALTER1
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);

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
		CLOG("Run at %dHz\n", IC_BOARD_FPGA_FIX_FREQ);
	else
		CLOG("Run at %dHz\n", IC_BOARD_FPGA_FIX_FREQ / (dev.sclk_div + 1) / 2);
#else
	if(dev.sclk_div == 0xFF)
		CLOG("Run at %dHz\n", CRM_GetFlashFreq());
	else
		CLOG("Run at %dHz\n", CRM_GetFlashFreq() / (dev.sclk_div + 1) / 2);
#endif

	uint32_t dat = inw(FLASHDL_BASE);
	dat &= ~(1UL << 17);
	outw(FLASHDL_BASE, dat);  // disable delay line

	//Force disable Flash1, enable Flash0
	outw(0x47600058, 0xffff0009);
	if(flash_init(&dev, 0, 0) != 0)
		CLOGD("FLash Init failed");

	//Force disable Flash0, enable Flash1
	outw(0x47600058, 0xffff0006);
	if(flash_init(&dev, 0, 0) != 0)
		CLOGD("FLash Init failed");

	// CS0-32K  CS1-32K
	outw(0x47600054, ((FLASH0_TEST_LEN >> 12) << 16) | FLASH0_TEST_START);

	static uint8_t flash0_buf[FLASH_TEST_SECTOR_SIZE * 2], flash0_buf2[FLASH_TEST_SECTOR_SIZE];
	static uint8_t flash1_buf[FLASH_TEST_SECTOR_SIZE * 2], flash1_buf2[FLASH_TEST_SECTOR_SIZE];
	static uint8_t buf[FLASH_TEST_SECTOR_SIZE];

	int test_times = 0;

	memcpy(&flash0_buf[FLASH_TEST_SECTOR_SIZE], flash0_buf, FLASH_TEST_SECTOR_SIZE);
	memcpy(&flash1_buf[FLASH_TEST_SECTOR_SIZE], flash1_buf, FLASH_TEST_SECTOR_SIZE);

	while(1)
//	for(int loop_cnt = 0; loop_cnt < 1; loop_cnt++)
	{
		for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i += 4)
		{
			*(int*)(&flash0_buf[i]) = rand();
		}

		for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i += 4)
		{
			*(int*)(&flash1_buf[i]) = (rand() + i);
		}

		uint8_t *flash0_pOpBuf = flash0_buf;
		uint8_t *flash1_pOpBuf = flash1_buf;

		FLASH_DEV dev_flash0 = dev;
		FLASH_DEV dev_flash1 = dev;

/************************************operate flash 0 start********************************************/
		//Force disable Flash1, enable Flash0
		outw(0x47600058, 0xffff0009);
		// Operation Flash0
		ret = flash_write_protection_set(&dev_flash0, false);

		uint32_t addr_curr = 0;
		ret = flash_erase(&dev_flash0, addr_curr, (FLASH_TEST_SECTOR_SIZE * 8));
		if(ret) {
			CLOG("Can't erase 0x%08x", addr_curr);
		}

		for(int j = 0; j < 8; j ++) {
			ret = flash_write(&dev_flash0, addr_curr, flash0_pOpBuf, FLASH_TEST_SECTOR_SIZE);
			addr_curr += FLASH_TEST_SECTOR_SIZE;
		}
		ret = flash_write_protection_set(&dev_flash0, true);
/************************************operate flash 0 end**********************************************/


/************************************operate flash 1 start********************************************/
		//Force disable Flash0, enable Flash1
		outw(0x47600058, 0xffff0006);
		// Operation Flash1
		ret = flash_write_protection_set(&dev_flash1, false);

		addr_curr = 0;
		ret = flash_erase(&dev_flash1, addr_curr, (FLASH_TEST_SECTOR_SIZE * 16));
		if(ret) {
			CLOG("Can't erase 0x%08x", addr_curr);
		}

		for(int k = 0; k < 8; k++) {
			ret = flash_write(&dev_flash1, addr_curr, flash1_pOpBuf, FLASH_TEST_SECTOR_SIZE);
			addr_curr += FLASH_TEST_SECTOR_SIZE;
		}
		ret = flash_write_protection_set(&dev_flash1, true);
/************************************operate flash 1 end**********************************************/

		// ram address select
		outw(0x47600058, 0xffff0000);

		// API read
		for(uint32_t addr_curr = DUAL_FLASH_TEST_START; addr_curr < (DUAL_FLASH_TEST_START + DUAL_FLASH_TEST_LEN); addr_curr += FLASH_TEST_SECTOR_SIZE)
		{
			ret = flash_read(&dev, addr_curr, buf, FLASH_TEST_SECTOR_SIZE);
			if(ret) {
				CLOG("Can't read 0x%08x", addr_curr);
			}

			int read_err = 0;
			uint8_t *compare_buf;

		    if (addr_curr < (DUAL_FLASH_TEST_START + 32 * 1024)) {
		        compare_buf = flash0_pOpBuf;
		    } else {
		        compare_buf = flash1_pOpBuf;
		    }

			for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i++)
			{
				if(compare_buf[i] != buf[i])
				{
					CLOG("0x%08x differs to read value 0x%08x at 0x%08x in API mode...", compare_buf[i], buf[i], (addr_curr + i + FLASH_ADDR_BASE));
				}
			}
		}

//		// XIP Read
		for(uint32_t addr_curr = DUAL_FLASH_TEST_START; addr_curr < (DUAL_FLASH_TEST_START + DUAL_FLASH_TEST_LEN); addr_curr++)
		{
		    // Skip last 16 bytes of flash0 (0x7FF0 - 0x7FFF)
		    if (addr_curr >= (0 + 32 * 1024 - 32) && addr_curr < (0 + 32 * 1024)) {
		        continue;
		    }

			uint8_t *compare_buf_xip;
			if(addr_curr < (DUAL_FLASH_TEST_START + 32 * 1024)) {
				compare_buf_xip = flash0_buf;
			} else {
				compare_buf_xip = flash1_buf;
			}
			uint32_t index = ((addr_curr - DUAL_FLASH_TEST_START) % FLASH_TEST_SECTOR_SIZE) + ((addr_curr - DUAL_FLASH_TEST_START) / ( FLASH_TEST_SECTOR_SIZE * 16) * 4);

			if (compare_buf_xip[index] != *((uint8_t *)(addr_curr + FLASH_ADDR_BASE))) {
				CLOG("0x%02x differs to read value 0x%02x at 0x%08x in XIP mode",
					 compare_buf_xip[index],
					 *((uint8_t *)(addr_curr + FLASH_ADDR_BASE)),
					 (addr_curr + FLASH_ADDR_BASE));
			}
		}
		test_times++;
		CLOG("Dual Flash Test %d", test_times);
	}
}

void dual_flash_single_read() {

	CLOG("dual_flash_single_read\n");

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

	// CS0 32K  CS1 32K
	outw(0x47600054, ((FLASH0_TEST_LEN >> 12) << 16) | FLASH0_TEST_START);

	//Config gpioA_16 CS1
	GPIOA_Handler = GPIOA();
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER1); // CSK_IOMUX_FUNC_ALTER1
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);

	static uint8_t flash0_buf[FLASH_TEST_SECTOR_SIZE * 2], flash0_buf2[FLASH_TEST_SECTOR_SIZE];
	static uint8_t flash1_buf[FLASH_TEST_SECTOR_SIZE * 2], flash1_buf2[FLASH_TEST_SECTOR_SIZE];
	static uint8_t buf[FLASH_TEST_SECTOR_SIZE];

	int test_times = 0;

	memcpy(&flash0_buf[FLASH_TEST_SECTOR_SIZE], flash0_buf, FLASH_TEST_SECTOR_SIZE);
	memcpy(&flash1_buf[FLASH_TEST_SECTOR_SIZE], flash1_buf, FLASH_TEST_SECTOR_SIZE);

	while(1) {

		for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i += 4)
		{
			*(int*)(&flash0_buf[i]) = 0xAAAAAAAA;
		}

		for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i += 4)
		{
			*(int*)(&flash1_buf[i]) = 0x55555555;
		}

		uint8_t *flash0_pOpBuf = flash0_buf;
		uint8_t *flash1_pOpBuf = flash1_buf;

		// API read
		for(uint32_t addr_curr = DUAL_FLASH_TEST_START; addr_curr < (DUAL_FLASH_TEST_START + DUAL_FLASH_TEST_LEN); addr_curr += FLASH_TEST_SECTOR_SIZE)
		{
			ret = flash_read(&dev, addr_curr, buf, FLASH_TEST_SECTOR_SIZE);
			if(ret) {
				CLOG("Can't read 0x%08x", addr_curr);
			}

			int read_err = 0;
			uint8_t *compare_buf;

		    if (addr_curr < (DUAL_FLASH_TEST_START + 32 * 1024)) {
		        compare_buf = flash0_pOpBuf;
		    } else {
		        compare_buf = flash1_pOpBuf;
		    }

			for(int i = 0; i < FLASH_TEST_SECTOR_SIZE; i++)
			{
				if(compare_buf[i] != buf[i])
				{
					CLOG("0x%08x differs to read value 0x%08x at 0x%08x in API mode...", compare_buf[i], buf[i], (addr_curr + i + FLASH_ADDR_BASE));
				}
			}
		}

		// XIP Read
		for(uint32_t addr_curr = DUAL_FLASH_TEST_START; addr_curr < (DUAL_FLASH_TEST_START + DUAL_FLASH_TEST_LEN); addr_curr++)
		{
			volatile uint8_t * volatile compare_buf_xip;
			if(addr_curr < (DUAL_FLASH_TEST_START + 32 * 1024)) {
				compare_buf_xip = flash0_buf;
			} else {
				compare_buf_xip = flash1_buf;
			}
			uint32_t index = ((addr_curr - DUAL_FLASH_TEST_START) % FLASH_TEST_SECTOR_SIZE) + ((addr_curr - DUAL_FLASH_TEST_START) / ( FLASH_TEST_SECTOR_SIZE * 16) * 4);

			if (compare_buf_xip[index] != *((uint8_t *)(addr_curr + FLASH_ADDR_BASE))) {
				CLOG("0x%02x differs to read value 0x%02x at 0x%08x in XIP mode",
					 compare_buf_xip[index],
					 *((uint8_t *)(addr_curr + FLASH_ADDR_BASE)),
					 (addr_curr + FLASH_ADDR_BASE));
			}
		}
		test_times++;
		CLOG("dual_flash_single_read %d", test_times);
	}
}

int main(void)
{
	logInit(0, 115200);
//	flash_test_erase_write_read();
	dual_flash_test();
//	dual_flash_single_read();
	while(1);
    return 0;
}
