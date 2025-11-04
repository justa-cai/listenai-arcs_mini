#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "log_print.h"

// IOMux Pad
#define IOMUX_IOA_TEST_PIN              4
#define IOMUX_IOB_TEST_PIN              6

static void iomux_funcselect_pinA_cfg(void) {
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, CSK_IOMUX_FUNC_ALTER13);
}

static void iomux_funcselect_pinB_cfg(void) {
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_ALTER8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_IOMUX_FUNC_ALTER13);
}

static void aon_iomux_funcselect_pinB_cfg(void) {
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_AON_IOMUX_FUNC_DEFAULT);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, CSK_AON_IOMUX_FUNC_ALTER3);
}

static void iomux_mode_pinA_cfg(void){
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_PULLUP_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_PULLDOWN_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_NONE_MODE);
}

static void iomux_mode_pinB_cfg(void){
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_PULLUP_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_PULLDOWN_MODE);
	IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_NONE_MODE);
}

static void iomux_forceset_pinA_cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_A, IOMUX_IOA_TEST_PIN, HAL_IOMUX_FORCE_OUT_HIGH);
}

static void iomux_forceset_pinB_cfg(void){
	IOMuxManager_PinForce(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_FORCE_OUT_LOW);
	IOMuxManager_PinForce(CSK_IOMUX_PAD_B, IOMUX_IOB_TEST_PIN, HAL_IOMUX_FORCE_OUT_HIGH);
}


typedef void (*function)(void);

typedef struct {
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
//		{iomux_funcselect_pinA_cfg, "iomux_funcselect_pinA_cfg"},
//		{iomux_funcselect_pinB_cfg, "iomux_funcselect_pinB_cfg"},
//		{aon_iomux_funcselect_pinB_cfg, "aon_iomux_funcselect_pinB_cfg"},
//		{iomux_mode_pinA_cfg, "iomux_mode_pinA_cfg"},
//		{iomux_mode_pinB_cfg, "iomux_mode_pinA_cfg"},
//		{iomux_forceset_pinA_cfg, "iomux_forceset_pinA_cfg"},
		{iomux_forceset_pinB_cfg, "iomux_forceset_pinB_cfg"},
};

int main(void) {
	logInit(0, 115200);
	CLOG("enter main: iomux test\r\n");
	uint32_t index;
	for(index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++) {
		test_case_t item = test_array[index];
		CLOG("test case : %s", item.name);
		item.function();
	}

	while(1);

	return 0;
}
