/*
 * qdec_test.c
 *
 *  Created on:
 *
 *
 */

/*
 * INCLUDES
 ****************************************************************************************
 */

#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "log_print.h"
#include "systick.h"

#include "Driver_QDEC.h"
#include "qdec_test.h"


#define DELAY_CNT_MAX (20)

typedef int32_t (*QDEC_TEST_OP_NORMAL)(void);

static struct QDEC_TEST_ITEM qdec_test_case_array[] = {

		{ "qdec_x_mode",	QDEC_OP_TYPE_NORMAL,	qdec_x_mode },
		{ "qdec_x_swap",	QDEC_OP_TYPE_NORMAL,	qdec_x_swap },
		{ "qdec_x_int_of",	QDEC_OP_TYPE_NORMAL,	qdec_x_int_of },
		{ "qdec_x_int_uf",	QDEC_OP_TYPE_NORMAL,	qdec_x_int_uf },
		{ "qdec_x_int_evt",	QDEC_OP_TYPE_NORMAL,	qdec_x_int_evt },
		{ "qdec_x_evt_th",	QDEC_OP_TYPE_NORMAL,	qdec_x_evt_th },

		{ "qdec_y_mode",	QDEC_OP_TYPE_NORMAL,	qdec_y_mode },
		{ "qdec_y_swap",	QDEC_OP_TYPE_NORMAL,	qdec_y_swap },
		{ "qdec_y_int_of",	QDEC_OP_TYPE_NORMAL,	qdec_y_int_of },
		{ "qdec_y_int_uf",	QDEC_OP_TYPE_NORMAL,	qdec_y_int_uf },
		{ "qdec_y_int_evt",	QDEC_OP_TYPE_NORMAL,	qdec_y_int_evt },
		{ "qdec_y_evt_th",	QDEC_OP_TYPE_NORMAL,	qdec_y_evt_th },

		{ "qdec_z_mode",	QDEC_OP_TYPE_NORMAL,	qdec_z_mode },
		{ "qdec_z_swap",	QDEC_OP_TYPE_NORMAL,	qdec_z_swap },
		{ "qdec_z_int_of",	QDEC_OP_TYPE_NORMAL,	qdec_z_int_of },
		{ "qdec_z_int_uf",	QDEC_OP_TYPE_NORMAL,	qdec_z_int_uf },
		{ "qdec_z_int_evt",	QDEC_OP_TYPE_NORMAL,	qdec_z_int_evt },
		{ "qdec_z_evt_th",	QDEC_OP_TYPE_NORMAL,	qdec_z_evt_th },

		{ "qdec_xyz_cnt",	QDEC_OP_TYPE_NORMAL,	qdec_xyz_cnt },
		{ "qdec_xyz_clear",	QDEC_OP_TYPE_NORMAL,	qdec_xyz_clear },

};

static struct QDEC_TEST_ITEM* qdec_test_get_next_entry(void);

static void delay(int n)
{
	SysTick_Delay_Ms(n);
	return;
}

volatile int32_t qdec_int_flag = 0;
void qdec_callback(uint32_t event, uint32_t param)
{
	qdec_int_flag = event;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();
	switch(event)
	{
	case QDEC_STATUS_X_EVENT:
        //CLOG("cbEvt x = %d\n", QDEC_Read_X(hQdec));
	    break;
    case QDEC_STATUS_X_OF:
        CLOG("cbEvt X Overflow, x = %d\n", QDEC_Read_X(hQdec));
        //QDEC_ClearCounter(hQdec, QDEC_AXES_X);
        break;
    case QDEC_STATUS_X_UF:
        CLOG("cbEvt X Underflow, x = %d\n", QDEC_Read_X(hQdec));
        //QDEC_ClearCounter(hQdec, QDEC_AXES_X);
        break;

    case QDEC_STATUS_Y_EVENT:
        //CLOG("cbEvt y = %d\n", QDEC_Read_Y(hQdec));
        break;
    case QDEC_STATUS_Y_OF:
        CLOG("cbEvt Y Overflow, y = %d\n", QDEC_Read_Y(hQdec));
        //QDEC_ClearCounter(hQdec, QDEC_AXES_Y);
        break;
    case QDEC_STATUS_Y_UF:
        CLOG("cbEvt Y Underflow, y = %d\n", QDEC_Read_Y(hQdec));
        //QDEC_ClearCounter(hQdec, QDEC_AXES_Y);
        break;

    case QDEC_STATUS_Z_EVENT:
        //CLOG("cbEvt z = %d\n", QDEC_Read_Z(hQdec));
        break;
    case QDEC_STATUS_Z_OF:
        CLOG("cbEvt Z Overflow, z = %d\n", QDEC_Read_Z(hQdec));
        //QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
        break;
    case QDEC_STATUS_Z_UF:
        CLOG("cbEvt Z Underflow, z = %d\n", QDEC_Read_Z(hQdec));
        //QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
        break;

//		case QDEC_STATUS_X_OF:
//		case QDEC_STATUS_X_UF:
//		case QDEC_STATUS_X_EVENT:
//		case QDEC_STATUS_Y_OF:
//		case QDEC_STATUS_Y_UF:
//		case QDEC_STATUS_Y_EVENT:
//		case QDEC_STATUS_Z_OF:
//		case QDEC_STATUS_Z_UF:
//		case QDEC_STATUS_Z_EVENT:
		default:
			break;
	}
	return;
}

static void qdec_io_init()
{
/*
	// Enable the pullup on the PB port
	AON_IOMuxManager_ModeConfigure(CSK_AON_IOMUX_PAD_B, 0, HAL_AON_IOMUX_PULLUP_MODE);
	AON_IOMuxManager_ModeConfigure(CSK_AON_IOMUX_PAD_B, 1, HAL_AON_IOMUX_PULLUP_MODE);
	AON_IOMuxManager_ModeConfigure(CSK_AON_IOMUX_PAD_B, 2, HAL_AON_IOMUX_PULLUP_MODE);
	AON_IOMuxManager_ModeConfigure(CSK_AON_IOMUX_PAD_B, 3, HAL_AON_IOMUX_PULLUP_MODE);
*/

	// X axis
/*
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER17);  // PA30 configured to function 17 - QDEC_X_A
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_ALTER17);  // PA31 configured to function 17 - QDEC_X_B
*/
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER17);  // PA30 configured to function 17 - QDEC_X_A
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER17);  // PA31 configured to function 17 - QDEC_X_B

	// Y axis
/*
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, CSK_IOMUX_FUNC_ALTER17);  // PB00 configured to function 17 - QDEC_Y_A
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_ALTER17);  // PB01 configured to function 17 - QDEC_Y_B
*/
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, CSK_IOMUX_FUNC_ALTER17);  // PB00 configured to function 17 - QDEC_Y_A
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, CSK_IOMUX_FUNC_ALTER17);  // PB01 configured to function 17 - QDEC_Y_B

	// Z axis
/*
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, CSK_IOMUX_FUNC_ALTER17);  // PB02 configured to function 17 - QDEC_Z_A
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, CSK_IOMUX_FUNC_ALTER17);  // PB03 configured to function 17 - QDEC_Z_B
*/
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER17);  // PB02 configured to function 17 - QDEC_Z_A
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER17);  // PB03 configured to function 17 - QDEC_Z_B

	return;
}


int main(void)
{
	logInit(0, 115200);
    enable_GINT();

	CLOG("----------------- QDEC Test -----------------\n");

	test_loop();
	CLOG("----------------- The End -------------------\n");

	while(1)
		;

	return 0;
}

void test_loop(void)
{
	QDEC_TEST_LOG("Keep rotating the device in a constant speed along one direction\n");

	qdec_io_init();

	for (int i = 0; i < QDEC_TEST_TIMES; i++)
	{
		struct QDEC_TEST_ITEM* next_entry = qdec_test_get_next_entry();

		QDEC_TEST_LOG("%d: %s\n", i+1, next_entry->case_name);

		switch(next_entry->op_type)
		{
			case QDEC_OP_TYPE_NORMAL:
			{
				QDEC_TEST_OP_NORMAL test_op = (QDEC_TEST_OP_NORMAL)(next_entry->test_op);
				if(0 != test_op())
				{
					QDEC_TEST_LOG("    ***failed***\n");
				}
				break;
			}
			case QDEC_OP_TYPE_OTHER:
			default:
				QDEC_TEST_LOG("    ***not implemented yet***\n");
				break;

		}
	}


}

static struct QDEC_TEST_ITEM* qdec_test_get_next_entry(void)
{
	static int entry_idx = 0;
	struct QDEC_TEST_ITEM* pEntry = &qdec_test_case_array [entry_idx];
	entry_idx++;
	entry_idx %= (sizeof(qdec_test_case_array)/sizeof(struct QDEC_TEST_ITEM));
	return pEntry;
}

static void error_handler(void)
{
	QDEC_HandleTypeDef *qdec_dev = QDEC_Instance();
	QDEC_Stop(qdec_dev, QDEC_AXES_ALL);
	QDEC_DeInit(qdec_dev);
	QDEC_TEST_LOG("    ***Run into an error***\n");
	while(1)
		;
	return;
}


int qdec_x_mode(void)
{
	int delay_cnt;
	int16_t cnt_x;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X1 mode
	CLOG("    set to X1 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeX = QDEC_MODE_X1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_X);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		CLOG("    x = %d\n", cnt_x);
	}

	//set to X2 mode
	CLOG("    set to X2 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeX = QDEC_MODE_X2;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_X);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		CLOG("    x = %d\n", cnt_x);
	}

	//set to X4 mode
	CLOG("    set to X4 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeX = QDEC_MODE_X4;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_X);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		CLOG("    x = %d\n", cnt_x);
	}

	return 0;
}

int qdec_x_swap(void){
	int delay_cnt;
	int16_t cnt_x;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to normal mode
	CLOG("    set to normal mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_X);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		CLOG("    x = %d\n", cnt_x);
	}

	//set to swapping mode
	CLOG("    set to axis swapping mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.SwapX = 1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_X);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		CLOG("    x = %d\n", cnt_x);
	}

	return 0;
}

int qdec_x_int_of(void){
	int16_t cnt_x;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X4 mode to speed up
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeX = QDEC_MODE_X4;
	hQdec->Init.IntSel = QDEC_INT_MSK_X_OF;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_X);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_x = QDEC_Read_X(hQdec);
			CLOG("    x = %d\n", cnt_x);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_X_OF)
		error_handler();

	return 0;
}

int qdec_x_int_uf(void){
	int16_t cnt_x;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X4 mode to speed up
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeX = QDEC_MODE_X4;
	hQdec->Init.IntSel = QDEC_INT_MSK_X_UF;
	hQdec->Init.SwapX = 1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_X);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_x = QDEC_Read_X(hQdec);
			CLOG("    x = %d\n", cnt_x);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_X_UF)
		error_handler();

	return 0;
}

int qdec_x_int_evt(void){
	int16_t cnt_x;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.IntSel = QDEC_STATUS_X_EVENT;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_X);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_x = QDEC_Read_X(hQdec);
			CLOG("    x = %d\n", cnt_x);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_X_EVENT)
		error_handler();

	return 0;
}

int qdec_x_evt_th(void){
	int16_t cnt_x;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.IntSel = QDEC_STATUS_X_EVENT;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	int cnt_delay;

	// maximum 255
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_X);
	QDEC_SetEvtThrd_X(hQdec, 255);
	QDEC_Start(hQdec, QDEC_AXES_X);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_X_EVENT)
		error_handler();
	cnt_x = QDEC_Read_X(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_X); // SHOULD STOP HERE!!
	CLOG("    threshold = 255; cycle = %d; counter = %d\n", cnt_delay, cnt_x);

	// 128
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_X);
	QDEC_SetEvtThrd_X(hQdec, 128);
	QDEC_Start(hQdec, QDEC_AXES_X);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_X_EVENT)
		error_handler();
	cnt_x = QDEC_Read_X(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_X); // SHOULD STOP HERE!!
	CLOG("    threshold = 128; cycle = %d; counter = %d\n", cnt_delay, cnt_x);

	// 2
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_X);
	QDEC_SetEvtThrd_X(hQdec, 2);
	QDEC_Start(hQdec, QDEC_AXES_X);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_X_EVENT)
		error_handler();
	cnt_x = QDEC_Read_X(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_X); // SHOULD STOP HERE!!
	CLOG("    threshold = 2; cycle = %d; counter = %d\n", cnt_delay, cnt_x);

	return 0;
}

int qdec_y_mode(void){
	int delay_cnt;
	int16_t cnt_y;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X1 mode
	CLOG("    set to X1 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeY = QDEC_MODE_X1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Y);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_y = QDEC_Read_Y(hQdec);
		CLOG("    y = %d\n", cnt_y);
	}

	//set to X2 mode
	CLOG("    set to X2 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeY = QDEC_MODE_X2;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Y);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_y = QDEC_Read_Y(hQdec);
		CLOG("    y = %d\n", cnt_y);
	}

	//set to X4 mode
	CLOG("    set to X4 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeY = QDEC_MODE_X4;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Y);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_y = QDEC_Read_Y(hQdec);
		CLOG("    y = %d\n", cnt_y);
	}

	return 0;
}

int qdec_y_swap(void){
	int delay_cnt;
	int16_t cnt_y;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to normal mode
	CLOG("    set to normal mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Y);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_y = QDEC_Read_Y(hQdec);
		CLOG("    y = %d\n", cnt_y);
	}

	//set to swapping mode
	CLOG("    set to axis swapping mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.SwapY = 1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Y);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_y = QDEC_Read_Y(hQdec);
		CLOG("    y = %d\n", cnt_y);
	}
	return 0;
}

int qdec_y_int_of(void){
	int16_t cnt_y;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X4 mode to speed up
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeY = QDEC_MODE_X4;
	hQdec->Init.IntSel = QDEC_INT_MSK_Y_OF;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_Y);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_y = QDEC_Read_Y(hQdec);
			CLOG("    y = %d\n", cnt_y);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_Y_OF)
		error_handler();

	return 0;
}

int qdec_y_int_uf(void){
	int16_t cnt_y;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X4 mode to speed up
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeY = QDEC_MODE_X4;
	hQdec->Init.IntSel = QDEC_INT_MSK_Y_UF;
	hQdec->Init.SwapY = 1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_Y);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_y = QDEC_Read_Y(hQdec);
			CLOG("    y = %d\n", cnt_y);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_Y_UF)
		error_handler();

	return 0;
}

int qdec_y_int_evt(void){
	int16_t cnt_y;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.IntSel = QDEC_STATUS_Y_EVENT;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_Y);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_y = QDEC_Read_Y(hQdec);
			CLOG("    y = %d\n", cnt_y);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_Y_EVENT)
		error_handler();

	return 0;
}

int qdec_y_evt_th(void){
	int16_t cnt_y;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.IntSel = QDEC_STATUS_Y_EVENT;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	int cnt_delay;

	// maximum 255
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_Y);
	QDEC_SetEvtThrd_Y(hQdec, 255);
	QDEC_Start(hQdec, QDEC_AXES_Y);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_Y_EVENT)
		error_handler();
	cnt_y = QDEC_Read_Y(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_Y); // SHOULD STOP HERE!!
	CLOG("    threshold = 255; cycle = %d; counter = %d\n", cnt_delay, cnt_y);

	// 128
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_Y);
	QDEC_SetEvtThrd_Y(hQdec, 128);
	QDEC_Start(hQdec, QDEC_AXES_Y);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_Y_EVENT)
		error_handler();
	cnt_y = QDEC_Read_Y(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_Y); // SHOULD STOP HERE!!
	CLOG("    threshold = 128; cycle = %d; counter = %d\n", cnt_delay, cnt_y);

	// 2
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_Y);
	QDEC_SetEvtThrd_Y(hQdec, 2);
	QDEC_Start(hQdec, QDEC_AXES_Y);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_Y_EVENT)
		error_handler();
	cnt_y = QDEC_Read_Y(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_Y); // SHOULD STOP HERE!!
	CLOG("    threshold = 2; cycle = %d; counter = %d\n", cnt_delay, cnt_y);

	return 0;
}

int qdec_z_mode(void){
	int delay_cnt;
	int16_t cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X1 mode
	CLOG("    set to X1 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeZ = QDEC_MODE_X1;

	QDEC_Stop(hQdec, QDEC_AXES_Z);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Z);
	QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    z = %d\n", cnt_z);
	}

	//set to X2 mode
	CLOG("    set to X2 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeZ = QDEC_MODE_X2;

	QDEC_Stop(hQdec, QDEC_AXES_Z);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Z);
	QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    z = %d\n", cnt_z);
	}

	//set to X4 mode
	CLOG("    set to X4 mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeZ = QDEC_MODE_X4;

	QDEC_Stop(hQdec, QDEC_AXES_Z);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Z);
	QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    z = %d\n", cnt_z);
	}

	return 0;
}

int qdec_z_swap(void){
	int delay_cnt;
	int16_t cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to normal mode
	CLOG("    set to normal mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Z);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    z = %d\n", cnt_z);
	}

	//set to swapping mode
	CLOG("    set to axis swapping mode\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.SwapZ = 1;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_Z);
	while(delay_cnt < DELAY_CNT_MAX)
	{
		delay(100);
		delay_cnt++;
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    z = %d\n", cnt_z);
	}

	return 0;
}

int qdec_z_int_of(void){
	int16_t cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X4 mode to speed up
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeZ = QDEC_MODE_X4;
	hQdec->Init.IntSel = QDEC_INT_MSK_Z_OF;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_Z);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_z = QDEC_Read_Z(hQdec);
			CLOG("    z = %d\n", cnt_z);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_Z_OF)
		error_handler();

	return 0;
}

int qdec_z_int_uf(void){
	int16_t cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	//set to X4 mode to speed up
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.ModeZ = QDEC_MODE_X4;
	hQdec->Init.SwapZ = 1;
	hQdec->Init.IntSel = QDEC_INT_MSK_Z_UF;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_Z);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_z = QDEC_Read_Z(hQdec);
			CLOG("    z = %d\n", cnt_z);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_Z_UF)
		error_handler();

	return 0;
}

int qdec_z_int_evt(void){
	int16_t cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.IntSel = QDEC_STATUS_Z_EVENT;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	qdec_int_flag = 0;
	QDEC_Start(hQdec, QDEC_AXES_Z);

	int cnt_delay = 0;
	while(qdec_int_flag == 0)
	{
		delay(1);
		cnt_delay++;
		if(cnt_delay%100 == 0)
		{
			cnt_z = QDEC_Read_Z(hQdec);
			CLOG("    z = %d\n", cnt_z);
		}
	}
	if(qdec_int_flag != QDEC_STATUS_Z_EVENT)
		error_handler();

	return 0;
}

int qdec_z_evt_th(void){
	int16_t cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));
	hQdec->Init.IntSel = QDEC_STATUS_Z_EVENT;

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	int cnt_delay;

	// maximum 255
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
	QDEC_SetEvtThrd_Z(hQdec, 255);
	QDEC_Start(hQdec, QDEC_AXES_Z);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_Z_EVENT)
		error_handler();
	cnt_z = QDEC_Read_Z(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_Z); // SHOULD STOP HERE!!
	CLOG("    threshold = 255; cycle = %d; counter = %d\n", cnt_delay, cnt_z);

	// 128
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
	QDEC_SetEvtThrd_Z(hQdec, 128);
	QDEC_Start(hQdec, QDEC_AXES_Z);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_Z_EVENT)
		error_handler();
	cnt_z = QDEC_Read_Z(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_Z); // SHOULD STOP HERE!!
	CLOG("    threshold = 128; cycle = %d; counter = %d\n", cnt_delay, cnt_z);

	// 2
	qdec_int_flag = 0;
	cnt_delay = 0;
	QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
	QDEC_SetEvtThrd_Z(hQdec, 2);
	QDEC_Start(hQdec, QDEC_AXES_Z);

	while(qdec_int_flag == 0)
	{
		cnt_delay++;
	}
	if(qdec_int_flag != QDEC_STATUS_Z_EVENT)
		error_handler();
	cnt_z = QDEC_Read_Z(hQdec);
	QDEC_Stop(hQdec, QDEC_AXES_Z); // SHOULD STOP HERE!!
	CLOG("    threshold = 2; cycle = %d; counter = %d\n", cnt_delay, cnt_z);

	return 0;
}

int qdec_xyz_cnt(void){
	int delay_cnt;
	int16_t cnt_x, cnt_y, cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	CLOG("    Enable XYZ axes\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_ALL);
	while(delay_cnt < 10)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		cnt_y = QDEC_Read_Y(hQdec);
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    x = %d, y = %d, z = %d\n", cnt_x, cnt_y, cnt_z);
	}

	return 0;
}

int qdec_xyz_clear(void){
	int delay_cnt;
	int16_t cnt_x, cnt_y, cnt_z;
	QDEC_HandleTypeDef *hQdec = (QDEC_HandleTypeDef *)QDEC_Instance();

	CLOG("    Clear in X->Y->Z->XYZ sequence\n");
	delay_cnt = 0;
	memset(&hQdec->Init, 0, sizeof(QDEC_InitTypeDef));

	QDEC_Stop(hQdec, QDEC_AXES_ALL);
	QDEC_DeInit(hQdec);
	if(CSK_DRIVER_OK != QDEC_Init(hQdec, qdec_callback))
		error_handler();

	QDEC_Start(hQdec, QDEC_AXES_ALL);
	while(delay_cnt < 10)
	{
		delay(100);
		delay_cnt++;
		cnt_x = QDEC_Read_X(hQdec);
		cnt_y = QDEC_Read_Y(hQdec);
		cnt_z = QDEC_Read_Z(hQdec);
		CLOG("    x = %d, y = %d, z = %d\n", cnt_x, cnt_y, cnt_z);

		if(delay_cnt == 2)
		{
			CLOG("    clear X\n");
			QDEC_ClearCounter(hQdec, QDEC_AXES_X);
		}
		if(delay_cnt == 4)
		{
			CLOG("    clear Y\n");
			QDEC_ClearCounter(hQdec, QDEC_AXES_Y);
		}
		if(delay_cnt == 6)
		{
			CLOG("    clear Z\n");
			QDEC_ClearCounter(hQdec, QDEC_AXES_Z);
		}
		if(delay_cnt == 8)
		{
			CLOG("    clear ALL\n");
			QDEC_ClearCounter(hQdec, QDEC_AXES_ALL);
		}
	}

	return 0;
}



