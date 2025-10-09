#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "nvs.h"
#include "cache.h"
#include "spiflash.h"
#include "clock_config.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "ClockManager.h"
#include "esp_heap_caps.h"
#include "lisa_log.h"
#include "flash_msg.h"
#include "arcs_flash.h"
#include "memap.h"
#include "systick.h"

// #define DUAL_FLASH   (0)

#define _EXT_CODE_ __attribute__((section (".psram.text")))


#define FLASHDL_BASE            (0x47700000)

#define FLASH0_START		    (0)
#define FLASH0_SIZE			    (1024 * 1024 * 16)
//CS1
#define CS1_PORT                (GPIOA())
#define CS1_PAD                 (CSK_IOMUX_PAD_A)
#define CS1_PIN                 (11)
#define CS1_FUNC                (CSK_IOMUX_FUNC_ALTER1)

#define delay_ms SysTick_Delay_Ms
// #define delay_ms

static FLASH_DEV flash0 = {
	.base_addr = CMN_FLASHC_BASE,
	.d_width = 4,
	.sclk_div = 0xFF, //divider is 1
	.run_mod = RUN_WITHOUT_INT,//RUN_WITH_INT//RUN_WITHOUT_INT
	.timeout = 2000000,
	.addr_bytes = 3,
	.addr_auto = 0,
};

#ifdef DUAL_FLASH
static FLASH_DEV flash1 = {
	.base_addr = CMN_FLASHC_BASE,
	.d_width = 4,
	.sclk_div = 0xFF, //divider is 1
	.run_mod = RUN_WITH_INT,//RUN_WITH_INT//RUN_WITHOUT_INT
	.timeout = 2000000,
	.addr_bytes = 3,
	.addr_auto = 0,
};
#endif

_EXT_CODE_ int arcs_flash_write(uint32_t addr, const void *data, size_t len){
	FLASH_DEV *dev;
	off_t offset;
	uint32_t reg_ctrl;
	int ret = 0;
#ifdef DUAL_FLASH
	if(addr >= (FLASH0_SIZE)){
		dev = &flash1;
		reg_ctrl = 0xffff0006;//Force disable Flash0, enable Flash1
		offset = addr - FLASH0_SIZE;
	}else if(addr >= 0){
		dev = &flash0;
		reg_ctrl = 0xffff0009;//Force disable Flash1, enable Flash0
		offset = addr;
	}else{
		ret = -1;
		goto EXIT;
	}
#else
	dev = &flash0;
	reg_ctrl = 0xffff0009;//Force disable Flash1, enable Flash0
	offset = addr;
#endif
#ifndef INSTALL_FW
	flash_msg_send(CMD_FLASH_WRITE);
#endif
	taskENTER_CRITICAL();
	disable_GINT();
	__disable_irq();
	__RWMB();
	__FENCE_I();
#ifdef DUAL_FLASH
	outw(0x47600058, reg_ctrl);
#endif
	delay_ms(20);
	flash_write_protection_set(dev, false);
	ret = flash_write(dev, offset, data, len);
	flash_write_protection_set(dev, true);
#ifdef DUAL_FLASH
	outw(0x47600058, 0xffff0000);
#endif
#ifndef INSTALL_FW
	flash_set_flash_flag();
#endif
	__enable_irq();
	enable_GINT();
	taskEXIT_CRITICAL();

EXIT:
	return ret;
}

_EXT_CODE_ int arcs_flash_erase(uint32_t addr, size_t len){
	FLASH_DEV *dev;
	off_t offset;
	uint32_t reg_ctrl;
	int ret = 0;
#ifdef DUAL_FLASH
	if(addr >= (FLASH0_SIZE)){
		dev = &flash1;
		reg_ctrl = 0xffff0006;//Force disable Flash0, enable Flash1
		offset = addr - FLASH0_SIZE;
	}else if(addr >= 0){
		dev = &flash0;
		reg_ctrl = 0xffff0009;//Force disable Flash1, enable Flash0
		offset = addr;
	}else{
		ret = -1;
		goto EXIT;
	}
#else
	dev = &flash0;
	reg_ctrl = 0xffff0009;//Force disable Flash1, enable Flash0
	offset = addr;
#endif
#ifndef INSTALL_FW
	flash_msg_send(CMD_FLASH_WRITE);
#endif
	taskENTER_CRITICAL();
	disable_GINT();
	__disable_irq();
	__RWMB();
	__FENCE_I();
#ifdef DUAL_FLASH
	outw(0x47600058, reg_ctrl);
#endif
	delay_ms(20);
	flash_write_protection_set(dev, false);
	ret = flash_erase(dev, offset, len);
	flash_write_protection_set(dev, true);
#ifdef DUAL_FLASH
	outw(0x47600058, 0xffff0000);
#endif
#ifndef INSTALL_FW
	flash_set_flash_flag();
#endif
	__enable_irq();
	enable_GINT();
	taskEXIT_CRITICAL();

EXIT:
	return ret;
}

_EXT_CODE_ int arcs_flash_read(uint32_t addr, void *data, size_t len)
{
	FLASH_DEV *dev;
	off_t offset;
	int ret = 0;

#ifdef DUAL_FLASH
	if(addr >= (FLASH0_SIZE)){
		dev = &flash1;
		offset = addr - FLASH0_SIZE;
	}else if(addr >= 0){
		dev = &flash0;
		offset = addr;
	}else{
		ret = -1;
		goto EXIT;
	}
#else
	dev = &flash0;
	offset = addr;
#endif
	
	ret = flash_read(dev, offset, data, len);

EXIT:
	return ret;
}

_EXT_CODE_ int arcs_flash_init(void){
	int ret = 0;

#ifdef DUAL_FLASH
	//Config CS1
	void* GPIOA_Handler = CS1_PORT;
	IOMuxManager_PinConfigure(CS1_PAD, CS1_PIN, CS1_FUNC);
	GPIO_Initialize(GPIOA_Handler, NULL, NULL);
#endif
	uint32_t dat = inw(FLASHDL_BASE);
	dat &= ~(1UL << 17);
	outw(FLASHDL_BASE, dat);  // disable delay line
#ifndef INSTALL_FW
	#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
		ipc_halt_peer_core();
	#else
		flash_msg_send(CMD_FLASH_WRITE);
	#endif
#endif
	taskENTER_CRITICAL();
	disable_GINT();
	__disable_irq();
    __RWMB();
    __FENCE_I();
#ifdef DUAL_FLASH
	//Force disable Flash1, enable Flash0
	outw(0x47600058, 0xffff0009);
	if(flash_init(&flash0, 0, 0) != 0)
		ret = 1;

	//Force disable Flash0, enable Flash1
	outw(0x47600058, 0xffff0006);
	if(flash_init(&flash1, 0, 0) != 0)
		ret |= 1<<1;
        
	//flash地址空间设置
	outw(0x47600054, ((FLASH0_SIZE >> 12) << 16) | FLASH0_START);
	outw(0x47600058, 0xffff0000);
#else
	delay_ms(20);
	if(flash_init(&flash0, 0, 0) != 0)
		ret = 1;
#endif

	__enable_irq();
	enable_GINT();
	taskEXIT_CRITICAL();
#ifndef INSTALL_FW
	#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
		ipc_resume_peer_core();
	#else
		flash_set_flash_flag();
	#endif
#endif
    return ret;
}



/***************************for test************************************/
//xip read test
#define FLASH_READ_START_ADDR  (FLASH_ADDR_BASE+0xD00000)
#define FLASH_READ1_START_ADDR (FLASH_ADDR_BASE+FLASH0_SIZE+0xD00000)
#define FLASH_READ_SIZE        (1024*1024*2)
_EXT_CODE_ void flash_read_speed_test(void){

    char *buf = heap_caps_aligned_alloc(32, FLASH_READ_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    CLOGI("[%s]start buf addr:%p", __FUNCTION__, buf);
    if(buf){
        uint32_t start = xTaskGetTickCount();
        memcpy(buf, (char *)FLASH_READ_START_ADDR, FLASH_READ_SIZE);
        HAL_FlushDCache_by_Addr((uint32_t *)buf, FLASH_READ_SIZE);
        CLOGI("[%s]flash0 read:%d KB cost:%lld ms", __FUNCTION__, FLASH_READ_SIZE/1024, xTaskGetTickCount()-start);

#ifdef DUAL_FLASH
        start = xTaskGetTickCount();
        memcpy(buf, (char *)FLASH_READ1_START_ADDR, FLASH_READ_SIZE);
        HAL_FlushDCache_by_Addr((uint32_t *)buf, FLASH_READ_SIZE);
        CLOGI("[%s]flash1 read:%d KB cost:%lld ms", __FUNCTION__, FLASH_READ_SIZE/1024, xTaskGetTickCount()-start);
        heap_caps_free(buf);
#endif
    }else{
        CLOGE("[%s] malloc:%d fail!!", __FUNCTION__, FLASH_READ_SIZE);
    }
    return;
}

//write test
#define FLASH_WRITE_START_ADDR  (FLASH_ADDR_BASE+0xFF8000)
#define FLASH_WRITE1_START_ADDR (FLASH_ADDR_BASE+FLASH0_SIZE+0xFF8000)
#define FLASH_WRITE_SIZE        (1024*8)
_EXT_CODE_ void flash_write_test(void){

	char *wbuf = heap_caps_aligned_alloc(32, FLASH_WRITE_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
	char *rbuf = heap_caps_aligned_alloc(32, FLASH_WRITE_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);

	CLOGI("dual_flash_write_test wbuf:0x%p rbuf:0x%p size:%d", wbuf, rbuf, FLASH_WRITE_SIZE);
	if(wbuf && rbuf){
		memset(wbuf, 0x5A, FLASH_WRITE_SIZE);
		//write flash
		if(arcs_flash_write(FLASH_WRITE_START_ADDR, wbuf, FLASH_WRITE_SIZE)<0){
			CLOGE("[%s]write addr:0x%x fail!!", __FUNCTION__, FLASH_WRITE_START_ADDR);
		}
#ifdef DUAL_FLASH
		if(arcs_flash_write(FLASH_WRITE1_START_ADDR, wbuf, FLASH_WRITE_SIZE)<0){
			CLOGE("[%s]write addr:0x%x fail!!", __FUNCTION__, FLASH_WRITE1_START_ADDR);
		}
#endif
		//校验
		uint32_t flash_addr = FLASH_WRITE_START_ADDR;
        memcpy(rbuf, (char *)flash_addr, FLASH_WRITE_SIZE);
        HAL_FlushDCache_by_Addr((uint32_t *)rbuf, FLASH_WRITE_SIZE);
		CLOGI("[1]flash0 write test %s", memcmp(wbuf, rbuf, FLASH_WRITE_SIZE) ? "fail" : "ok");
		CLOGI("[2]flash0 write test %s", memcmp(wbuf, (char *)flash_addr, FLASH_WRITE_SIZE) ? "fail" : "ok");
#ifdef DUAL_FLASH
		flash_addr = FLASH_WRITE1_START_ADDR;
        memcpy(rbuf, (char *)flash_addr, FLASH_WRITE_SIZE);
        HAL_FlushDCache_by_Addr((uint32_t *)rbuf, FLASH_WRITE_SIZE);
		CLOGI("[1]flash1 write test %s", memcmp(wbuf, rbuf, FLASH_WRITE_SIZE) ? "fail" : "ok");
		CLOGI("[2]flash1 write test %s", memcmp(wbuf, (char *)flash_addr, FLASH_WRITE_SIZE) ? "fail" : "ok");
#endif
		heap_caps_free(wbuf);
		heap_caps_free(rbuf);
	}else{
		CLOGE("arcs_flash_write malloc:%d fail!!", FLASH_WRITE_SIZE);
	}
	return;
}

