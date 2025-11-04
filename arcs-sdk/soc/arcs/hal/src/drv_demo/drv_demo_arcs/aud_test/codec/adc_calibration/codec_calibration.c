#include "arcs_ap.h"
#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "ClockManager.h"

int main(void )
{
	uint32_t rdata;

	logInit(0, 115200);

	//Enable Audio Clock
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	__HAL_CRM_ADC_CLK_ENABLE();

	//debug port config
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.all = 0x8D2;
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_IN_SEL = 1;
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELB = 1;
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA =2;        //REG_AUD_R1_GLOBAL0.all = 0x813
	//Debug clk
	IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31;
	IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_CLK_EN  = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_06.bit.PAD_GPIOA_06_FSEL = 18;     //dbg_clk
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 17;     //calib_comp
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.AUD_EN_IREF = 1;
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.AUD_EN_VMID = 1;
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.EN_MICBIAS = 1;
	IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.bit.EN_MIC_CAPLESS = 1;

	//cali config
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.AUD_ADC_CLK_INV = 0x1;

	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 1;

	//IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.all =0x1613;

	//IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.all =0x613;

	//while(IP_AUDIO_CODEC->REG_AUD_R25_STATUS0.bit.ADC_RC_CALI_DONE != 0x1);
	//if(IP_AUDIO_CODEC->REG_AUD_R25_STATUS0.bit.ADC_RC_CALI_FAIL != 0x1){
	//    rdata = IP_AUDIO_CODEC->REG_AUD_R25_STATUS0.bit.ADC_QUAR_COV;
	//    CLOGD("RC Calibration Done! ADC_QUAR_COV=0x%x\n", rdata);
	//}
	//else{
	//    CLOGD("RC Calibration Failed!\n");
	//    c_fail( );
	//}

	//   AON_CODEC->REG_AUD_R2_GLOBAL1.all  = 0x564;        //vmid en, iref en
	//   IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.all = 0x2613;
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADC_CAP_CALI_GO = 1;
	CLOGD("Calibration Go\n");
	//  AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.all = 0x613;
	IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADC_CAP_CALI_GO = 0;
	CLOGD("Waiting Calibration Done\n");
	while(IP_AUDIO_CODEC->REG_AUD_R25_STATUS0.bit.ADC_CAP_CALI_DONE != 0x1);
	CLOGD("Calibration Done\n");
	if(IP_AUDIO_CODEC->REG_AUD_R25_STATUS0.bit.ADC_CAP_CALI_FAIL != 0x1){
		rdata = IP_AUDIO_CODEC->REG_AUD_R25_STATUS0.bit.AUD_ADC_CAP_CALI;
		CLOGD("CAP Calibration Done! ADC_CAP_CALI=0x%x\n", rdata);
	}
	else{
		CLOGD("CAP Calibration Failed!\n");
		//c_fail( );
	}
	CLOGD("Calibration pass\n");

	while(1);
}
