#include "marvell88w8801_core.h"
#include "ftsdc021.h"
#include "hw_sdio.h"

mrvl88w8801_core_t mrvl88w8801_core;
pmrvl88w8801_core_t pmrvl88w8801_core = &mrvl88w8801_core;


uint8_t mp_reg_array[256];
uint8_t mrvl_tx_buffer[TX_BUFFER_SIZE];
uint8_t mrvl_rx_buffer[RX_BUFFER_SIZE];

//extern const uint8_t mrvl88w8801_fw[255536];
const uint8_t mrvl88w8801_fw[256];
//extern psdio_core_t phw_sdio_core;
wifi_cb_t *mrvl_wifi_cb = NULL;

extern void ethernet_sta_driver_init(uint8_t *mac_address);
extern void ethernet_link_up(void);
extern void ethernet_link_down(void);
extern void ethernet_dhcp_start(void);
extern void ethernet_lwip_process(uint8_t *rx,uint16_t rx_len);
extern void ethernet_dhcpd_start(void* hook);
extern void ethernet_dhcpd_stop(void);


/* ���������� */
static uint8_t mrvl88w8801_core_init(void);
static uint8_t mrvl88w8801_download_fw(void);
static uint8_t mrvl88w8801_enable_host_int(void);
static uint8_t mrvl88w8801_init_fw(void);
static uint8_t mrvl88w8801_download_fw_wait(void);
static uint8_t mrvl88w8801_get_control_io_port(void);
static uint8_t mrvl88w8801_get_fw_status(uint16_t *fw_status);
static uint8_t mrvl88w8801_get_hw_spec(uint8_t* tx);
static uint8_t mrvl88w8801_ret_get_hw_spec(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_mac_addr_prepare(uint8_t* tx,uint16_t cmd_action,void *data_buff,uint16_t data_len);
static uint8_t mrvl88w8801_supplicant_pmk_prepare(uint8_t* tx,uint16_t cmd_action,void *data_buff,uint16_t data_len);
static uint8_t mrvl88w8801_sys_conf_prepare(uint8_t* tx,uint16_t cmd_action,void *data_buff,uint16_t data_len);
static uint8_t mrvl88w8801_cre_ap_prepare(uint8_t* tx);
static uint8_t mrvl88w8801_ass_supplicant_pmk_pkg(uint8_t *bssid);
static uint8_t mrvl88w8801_mac_control(uint8_t* tx,void *data_buff);
static uint8_t mrvl88w8801_func_init(uint8_t* tx);
static uint8_t mrvl88w8801_scan_prepare(uint8_t* tx,void *data_buff,uint16_t data_len);
static uint8_t mrvl88w8801_connect_prepare(uint8_t* tx,void *data_buff,uint16_t data_len);
static uint8_t mrvl88w8801_ret_scan(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_ret_connect(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_ret_mac_address(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_add_mac_address(uint8_t *mac_address);
static uint8_t mrvl88w8801_del_mac_address(uint8_t *mac_address);
static uint8_t mrvl88w8801_handle_interrupt(void);
static uint8_t mrvl88w8801_get_read_port(uint8_t *port);
static uint8_t mrvl88w8801_get_write_port(uint8_t *port);
static uint8_t mrvl88w8801_parse_rx_packet(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_prepare_cmd(uint16_t cmd_id,uint16_t cmd_action,void *data_buff,uint16_t data_len);
static uint8_t mrvl88w8801_process_cmdrsp(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_process_event(uint8_t *rx_buffer,int len);
static uint8_t mrvl88w8801_process_data(uint8_t *rx_buffer,int len);


uint8_t hw_sdio_cmd52(uint8_t write,uint8_t func_num,uint32_t address,uint8_t para,uint8_t *resp)
{

	CLOGD("hw_sdio_cmd52\n\r");
	gm_sdc_api_sdio_cmd52(0, write, func_num, address, para, resp);

	return 0;
}

uint8_t hw_sdio_cmd53(uint8_t write, uint8_t func_num,uint32_t address, uint8_t incr_addr, uint8_t *buf,uint32_t size)
{
	CLOGD("hw_sdio_cmd53: %d; %d; %x; %d; %x; %d \n\r", write, func_num, address, incr_addr, buf, size);

	gm_sdc_api_sdio_cmd53(0, write, func_num, address, incr_addr, (u32*) buf, size, 512);
}



/******************************************************************************
 *	������:	mrvl88w8801_init
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		Marvell88w8801�ĳ�ʼ��(��ȡIO PORT,download fw,�����жϣinit cmd)
******************************************************************************/
uint8_t mrvl88w8801_init(wifi_cb_t *cb)
{
    mrvl_wifi_cb = cb;
    /* core init */
    mrvl88w8801_core_init();
    /* Get control io port */
    mrvl88w8801_get_control_io_port();
    /* download fw */
    mrvl88w8801_download_fw();
    /* enable interupt */
    mrvl88w8801_enable_host_int();
    /* init fw cmd */
    mrvl88w8801_init_fw();


    return COMP_ERR_OK;
}

uint8_t mrvl88w8801_deinit(void)
{
	if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_init_result)
        mrvl_wifi_cb->wifi_init_result(1);
	
	return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_process_packet
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		�������жϷ���ʱ����ȡ������ҽ���
******************************************************************************/
uint8_t mrvl88w8801_process_packet()
{
    uint32_t control_io_port = pmrvl88w8801_core->control_io_port;
    uint8_t port;
    uint8_t int_status;
    uint32_t len_reg_l, len_reg_u;
    uint16_t rx_len;
//lyt:
//    phw_sdio_core->int_occur = 0;
    uint8_t* rx_buffer = mrvl_rx_buffer;

    /* �����ж� */
    mrvl88w8801_handle_interrupt();

    /* ��ȡ�жϼĴ���״̬ */
    int_status = pmrvl88w8801_core->mp_regs[HOST_INT_STATUS_REG];
    if((int_status & UP_LD_HOST_INT_MASK) == UP_LD_HOST_INT_MASK)
    {
        while(1)
        {
            /* ���read port */
            if(mrvl88w8801_get_read_port(&port) != COMP_ERR_OK)
            {
                //COMP_DEBUG("no more rd_port to be handled\n");
                break;
            }

            /* ��ȡ��Ҫ��ȡ�ķ������ */
            len_reg_l = RD_LEN_P0_L + (port << 1);
            len_reg_u = RD_LEN_P0_U + (port << 1);
            rx_len = ((uint16_t) pmrvl88w8801_core->mp_regs[len_reg_u]) << 8;
            rx_len |= (uint16_t) pmrvl88w8801_core->mp_regs[len_reg_l];
            //COMP_DEBUG("@@@@@@@RX: port=%d rx_len=%u\n", port, rx_len);

            if(rx_buffer != NULL)
            {
                /* ʹ��CMD53��ȡ�յ������� */
                hw_sdio_cmd53(SDIO_EXCU_READ,SDIO_FUNC_1,control_io_port+port,0,rx_buffer,rx_len);
                //COMP_DEBUG("RECV DATA\n");
                //hw_hex_dump(rx_buffer,rx_len);
                /* �������� */
                mrvl88w8801_parse_rx_packet(rx_buffer,rx_len);
            }
        }
    }

    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_process_packet
 * ����:  		channel(IN)		-->channel����,���ڴ洢�ֱ�����ʲôͨ��
 				channel_num(IN)	-->������ͨ������
 				max_time(IN)		-->���������time
 * ����ֵ: 	����ִ�н��
 * ����:		ִ����ͨ��������
******************************************************************************/
uint8_t mrvl88w8801_scan(uint8_t *channel,uint8_t channel_num,uint16_t max_time)
{
    uint8_t index = 0;
    uint16_t scan_para_len = sizeof(MrvlIEtypesHeader_t) + channel_num*sizeof(ChanScanParamSet_t);
    uint8_t scan_para[scan_para_len];
    MrvlIEtypes_ChanListParamSet_t *channel_list;
    ChanScanParamSet_t *channel_list_para[channel_num];

    COMP_DEBUG("mrvl88w8801_scan\n");
    channel_list = (MrvlIEtypes_ChanListParamSet_t *)(scan_para);
    channel_list->header.type = TLV_TYPE_CHANLIST;
    channel_list->header.len = channel_num*sizeof(ChanScanParamSet_t);

    /* ���channel list�Ĳ��� */
    for(index = 0; index < channel_num; index++)
    {
        channel_list_para[index] = (ChanScanParamSet_t *)(scan_para + sizeof(MrvlIEtypesHeader_t)+index*sizeof(ChanScanParamSet_t));
        channel_list_para[index]->radio_type = 0;
        channel_list_para[index]->chan_number = channel[index];
        channel_list_para[index]->chan_scan_mode = 0;
        channel_list_para[index]->min_scan_time = 0;
        channel_list_para[index]->max_scan_time = max_time;
    }

    mrvl88w8801_prepare_cmd(HostCmd_CMD_802_11_SCAN,HostCmd_ACT_GEN_GET,&scan_para,scan_para_len);
    return COMP_ERR_OK;

}

/******************************************************************************
 *	������:	mrvl88w8801_scan_ssid
 * ����:  		ssid(IN)		-->�ض�������AP����
 				ssid_len(IN)	-->�ض�����AP���Ƶĳ���
 				max_time(IN)	-->���������time
 * ����ֵ: 	����ִ�н��
 * ����:		ִ���ض�������������
******************************************************************************/
uint8_t mrvl88w8801_scan_ssid(uint8_t *ssid,uint8_t ssid_len,uint16_t max_time)
{
    uint8_t index = 0;
    uint16_t scan_para_len = sizeof(MrvlIEtypesHeader_t) + ssid_len +sizeof(MrvlIEtypesHeader_t) + 14*sizeof(ChanScanParamSet_t);
    uint8_t scan_para[scan_para_len];
    MrvlIEtypes_ChanListParamSet_t *channel_list;
    ChanScanParamSet_t *channel_list_para[14];
    MrvlIEtypes_SSIDParamSet_t *ssid_tlv =  (MrvlIEtypes_SSIDParamSet_t *)scan_para;

    COMP_DEBUG("mrvl88w8801_scan_ssid\n");
    /* ���SSID tlv�Ĳ��� */
    ssid_tlv->header.type = TLV_TYPE_SSID;
    ssid_tlv->header.len = ssid_len;
    comp_memcpy(ssid_tlv->ssid,ssid,ssid_len);

    /* ���channel list tlv�Ĳ��� */
    channel_list = (MrvlIEtypes_ChanListParamSet_t *)(scan_para+sizeof(MrvlIEtypesHeader_t) + ssid_len);
    channel_list->header.type = TLV_TYPE_CHANLIST;
    channel_list->header.len = 14*sizeof(ChanScanParamSet_t);
    for(index = 0; index < 14; index++)
    {
        channel_list_para[index] = (ChanScanParamSet_t *)(scan_para + sizeof(MrvlIEtypesHeader_t)*2+ssid_len+index*sizeof(ChanScanParamSet_t));

        channel_list_para[index]->radio_type = 0;
        channel_list_para[index]->chan_number = index+1;
        channel_list_para[index]->chan_scan_mode = 0;
        channel_list_para[index]->min_scan_time = 0;
        channel_list_para[index]->max_scan_time = max_time;
    }

    mrvl88w8801_prepare_cmd(HostCmd_CMD_802_11_SCAN,HostCmd_ACT_GEN_GET,&scan_para,scan_para_len);
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_connect
 * ����:  		ssid(IN)		-->Ҫ���ӵ�AP����
 				ssid_len(IN)	-->Ҫ���ӵ�AP���Ƴ���
 				pwd(IN)		-->Ҫ���ӵ�AP����
 				pwd_len(IN)	-->Ҫ���ӵ�AP���볤��
 * ����ֵ: 	����ִ�н��
 * ����:		����ssid
******************************************************************************/
uint8_t mrvl88w8801_connect(uint8_t *ssid,uint8_t ssid_len,uint8_t *pwd,uint8_t pwd_len)
{
    comp_memcpy(pmrvl88w8801_core->con_info.ssid,ssid,ssid_len);
    pmrvl88w8801_core->con_info.ssid_len = ssid_len;
    comp_memcpy(pmrvl88w8801_core->con_info.pwd,pwd,pwd_len);
    pmrvl88w8801_core->con_info.pwd_len = pwd_len;

    /* ִ���ض����Ƶ�������������������������������������У������con_statusȥ���� */
    mrvl88w8801_scan_ssid(ssid,ssid_len,200);
    pmrvl88w8801_core->con_info.con_status = CON_STA_CONNECTING;
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_disconnect
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		������ΪSTA mode�������Ͽ���AP������
******************************************************************************/
uint8_t mrvl88w8801_disconnect()
{
    mrvl88w8801_prepare_cmd(HostCmd_CMD_802_11_DEAUTHENTICATE,HostCmd_ACT_GEN_GET,NULL,0);
    return COMP_ERR_OK;
}

/* TODO:дһ��API��ʾ���Ե�AP */
/******************************************************************************
 *	������:	mrvl88w8801_disconnect_sta
 * ����:  		index(IN)			-->����Ҫ�Ͽ���STA index
 * ����ֵ: 	����ִ�н��
 * ����:		������ΪAP mode���ߵ�ĳһ��STA������
******************************************************************************/
uint8_t mrvl88w8801_disconnect_sta(uint8_t *mac_address)
{
    mrvl88w8801_prepare_cmd(HOST_CMD_APCMD_STA_DEAUTH,HostCmd_ACT_GEN_GET,mac_address,MAC_ADDR_LENGTH);
    mrvl88w8801_del_mac_address(mac_address);
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_get_send_data_buf
 * ����:  		NULL
 * ����ֵ: 	����TX buffer��payload��Ҳ����tcp/ip���ݲ���
 * ����:		����tcp/ip���ã�ֱ��дtcp/ip����
******************************************************************************/
uint8_t *mrvl88w8801_get_send_data_buf()
{
    TxPD *tx_packet = (TxPD *)(mrvl_tx_buffer);
    pmrvl88w8801_core->tx_data_ptr = mrvl_tx_buffer;
    return (tx_packet->payload);
}

/******************************************************************************
 *	������:	mrvl88w8801_send_date
 * ����:  		data(IN)			-->Ҫ���͵�data
 				size(IN)			-->Ҫ���͵�data size
 * ����ֵ: 	����ִ�н��
 * ����:		����data
******************************************************************************/
uint8_t mrvl88w8801_send_date(uint8_t *data,uint16_t size)
{
    uint8_t wr_bitmap_l;
    uint8_t wr_bitmap_u;
    uint8_t port = 0;
    TxPD *tx_packet = (TxPD *)(pmrvl88w8801_core->tx_data_ptr);
    uint16_t tx_packet_len = sizeof(TxPD) - sizeof(tx_packet->payload) + size;

    tx_packet->pack_len = tx_packet_len;
    tx_packet->pack_type = TYPE_DATA;

    tx_packet->bss_type = pmrvl88w8801_core->bss_type;
    tx_packet->bss_num = 0;
    tx_packet->tx_pkt_length = size;
    tx_packet->tx_pkt_offset = tx_packet_len - size - 4;
    tx_packet->tx_pkt_type = 0;
    tx_packet->tx_control = 0;
    tx_packet->priority = 0;
    tx_packet->flags = 0;
    tx_packet->pkt_delay_2ms = 0;
    tx_packet->reserved1 = 0;
    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,WR_BITMAP_L,0,&wr_bitmap_l);
    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,WR_BITMAP_U,0,&wr_bitmap_u);
    pmrvl88w8801_core->write_bitmap = wr_bitmap_l;
    pmrvl88w8801_core->write_bitmap |=wr_bitmap_u << 8;
    /* ���write port������ */
    mrvl88w8801_get_write_port(&port);
    //COMP_DEBUG("SEND DAT PORT %d\n",port);
    //COMP_DEBUG("SEND DAT\n");
    //hw_hex_dump((uint8_t*)tx_packet,tx_packet->pack_len);
    hw_sdio_cmd53(SDIO_EXCU_WRITE,SDIO_FUNC_1,pmrvl88w8801_core->control_io_port+port,0,(uint8_t*)tx_packet,tx_packet->pack_len);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_cre_ap
 * ����:  		ssid(IN)				-->Ҫ������AP����
 				ssid_len(IN)			-->Ҫ������AP���Ƴ���
 				pwd(IN)				-->Ҫ������AP����
 				pwd_len(IN)			-->Ҫ������AP���볤��
 				sec_type(IN)			-->Ҫ������AP��֤����
 											WIFI_SECURITYTYPE_NONE = 0,
    										WIFI_SECURITYTYPE_WEP = 1,
    										WIFI_SECURITYTYPE_WPA = 2,
    										WIFI_SECURITYTYPE_WPA2 = 3
    			broadcast_ssid(IN)	-->�������ȵ��Ƿ��ܱ�������
 * ����ֵ: 	����ִ�н��
 * ����:		����AP�ȵ�,����˲����ĸ���������������ô��ֱ�Ӵ���һ��Ĭ��
 				���Ʋ���û�м������͵��ȵ�(Marvell Micro AP)
 				���ֻ�Ǵ�ssid,ssid_len����ô�ᴴ��һ��ssid���Ƶ��ȵ�
 				���������������ô�ᰴ������Ҫ�Ĵ����ȵ�
******************************************************************************/
uint8_t mrvl88w8801_cre_ap(uint8_t *ssid,uint8_t ssid_len,uint8_t *pwd,uint8_t pwd_len,uint8_t sec_type,uint8_t broadcast_ssid)
{
    uint8_t sys_config[256];
    uint8_t sys_config_len = 0;

    /* ���SSID��TLV */
    if(ssid != NULL && ssid_len != 0 && ssid_len < MAX_SSID_LENGTH)
    {
        MrvlIEtypes_SSIDParamSet_t *ssid_tlv;
        MrvlIETypes_ApBCast_SSID_Ctrl_t *ssid_broadcast_tlv;
        ssid_tlv	=  (MrvlIEtypes_SSIDParamSet_t *)sys_config;
        ssid_tlv->header.type = TLV_TYPE_SSID;
        ssid_tlv->header.len = ssid_len;
        comp_memcpy(ssid_tlv->ssid,ssid,ssid_len);
        sys_config_len += sizeof(MrvlIEtypesHeader_t) + ssid_len;

        ssid_broadcast_tlv = (MrvlIETypes_ApBCast_SSID_Ctrl_t *)(sys_config+sys_config_len);
        ssid_broadcast_tlv->header.type = TLV_TYPE_UAP_BCAST_SSID_CTL;
        ssid_broadcast_tlv->header.len = sizeof(uint8_t);
        ssid_broadcast_tlv->broadcast_ssid = broadcast_ssid;
        sys_config_len += sizeof(MrvlIETypes_ApBCast_SSID_Ctrl_t);
    }

    /* ��� >= WIFI_SECURITYTYPE_WPA,�����Ҫ����wpa/wpa2����֤���͵��ȵ� */
    if(sec_type >= WIFI_SECURITYTYPE_WPA)
    {
        MrvlIEtypes_Enc_Protocol_t *enc_protocol_tlv;
        MrvlIEtypes_AKMP_t *akmp_tlv;
        MrvlIEtypes_PTK_cipher_t *ptk_cipher_tlv;
        MrvlIEtypes_GTK_cipher_t *gtk_cipher_tlv;
        if(pwd != NULL && pwd_len != 0 && pwd_len < MAX_PHRASE_LENGTH)
        {
            MrvlIEtypes_PASSPhrase_t *phrase_tlv;


            phrase_tlv = (MrvlIEtypes_PASSPhrase_t *)(sys_config+sys_config_len);
            phrase_tlv->header.type = TLV_TYPE_UAP_WPA_PASSPHRASE;
            phrase_tlv->header.len = pwd_len;
            comp_memcpy(phrase_tlv->phrase,pwd,pwd_len);
            sys_config_len += sizeof(MrvlIEtypesHeader_t) + pwd_len;
        }

        enc_protocol_tlv = (MrvlIEtypes_Enc_Protocol_t *)(sys_config+sys_config_len);
        enc_protocol_tlv->header.type = TLV_TYPE_UAP_ENCRYPT_PROTOCOL;
        enc_protocol_tlv->header.len = sizeof(uint16_t);
        if(sec_type == WIFI_SECURITYTYPE_WPA)
            enc_protocol_tlv->enc_protocol = 1<<3;
        if(sec_type == WIFI_SECURITYTYPE_WPA2)
            enc_protocol_tlv->enc_protocol = 1<<5;
        sys_config_len += sizeof(MrvlIEtypes_Enc_Protocol_t);

        akmp_tlv = (MrvlIEtypes_AKMP_t *)(sys_config+sys_config_len);
        akmp_tlv->header.type = TLV_TYPE_UAP_AKMP;
        akmp_tlv->header.len = sizeof(MrvlIEtypes_AKMP_t) - sizeof(MrvlIEtypesHeader_t);
        akmp_tlv->key_mgmt = 1<<1;
        akmp_tlv->operation = 0;
        sys_config_len += sizeof(MrvlIEtypes_AKMP_t);

        ptk_cipher_tlv = (MrvlIEtypes_PTK_cipher_t *)(sys_config+sys_config_len);
        ptk_cipher_tlv->header.type = TLV_TYPE_PWK_CIPHER;
        ptk_cipher_tlv->header.len = sizeof(MrvlIEtypes_PTK_cipher_t) - sizeof(MrvlIEtypesHeader_t);
        if(sec_type == WIFI_SECURITYTYPE_WPA)
            ptk_cipher_tlv->protocol = 1<<3;
        if(sec_type == WIFI_SECURITYTYPE_WPA2)
            ptk_cipher_tlv->protocol = 1<<5;
        ptk_cipher_tlv->cipher = WPA_CIPHER_CCMP;
        sys_config_len += sizeof(MrvlIEtypes_PTK_cipher_t);

        gtk_cipher_tlv = (MrvlIEtypes_GTK_cipher_t *)(sys_config+sys_config_len);
        gtk_cipher_tlv->header.type = TLV_TYPE_GWK_CIPHER;
        gtk_cipher_tlv->header.len = sizeof(MrvlIEtypes_GTK_cipher_t) - sizeof(MrvlIEtypesHeader_t);
        gtk_cipher_tlv->cipher = WPA_CIPHER_CCMP;
        sys_config_len += sizeof(MrvlIEtypes_GTK_cipher_t);
    }
    if(sys_config_len != 0)
        mrvl88w8801_prepare_cmd(HOST_CMD_APCMD_SYS_CONFIGURE,HostCmd_ACT_GEN_SET,&sys_config,sys_config_len);
    else
        mrvl88w8801_prepare_cmd(HOST_CMD_APCMD_BSS_START,HostCmd_ACT_GEN_GET,NULL,0);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_stop_ap
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		ֹͣһ��AP�ȵ�
******************************************************************************/
uint8_t mrvl88w8801_stop_ap()
{
    mrvl88w8801_prepare_cmd(HOST_CMD_APCMD_BSS_STOP,HostCmd_ACT_GEN_GET,NULL,0);
    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_show_sta
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		��ӡ��ǰ���ӵ�STA index��mac address
******************************************************************************/
uint8_t mrvl88w8801_show_sta(void)
{
    uint8_t index = 0;

    for(index = 0; index < WIFI_CONF_MAX_CLIENT_NUM; index++)
    {
        if(pmrvl88w8801_core->sta_mac[index].used == 1)
        {
            COMP_DEBUG("STA INDEX %02d MAC:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n",index,
                       pmrvl88w8801_core->sta_mac[index].sta_mac_address[0],
                       pmrvl88w8801_core->sta_mac[index].sta_mac_address[1],
                       pmrvl88w8801_core->sta_mac[index].sta_mac_address[2],
                       pmrvl88w8801_core->sta_mac[index].sta_mac_address[3],
                       pmrvl88w8801_core->sta_mac[index].sta_mac_address[4],
                       pmrvl88w8801_core->sta_mac[index].sta_mac_address[5]);
        }
    }
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_ret_get_hw_spec
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		����GET HW SPEC�������Ӧ���˲�������ֻ�ǻ�ȡwrite data port
 				�����ֵ
******************************************************************************/
static uint8_t mrvl88w8801_ret_get_hw_spec(uint8_t *rx_buffer,int len)
{
    HostCmd_DS_GET_HW_SPEC *hw_spec = (HostCmd_DS_GET_HW_SPEC *)rx_buffer;

    pmrvl88w8801_core->mp_end_port = hw_spec->mp_end_port;
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_ass_supplicant_pmk_pkg
 * ����:  		bssid(IN)				-->mac address
 * ����ֵ: 	����ִ�н��
 * ����:		���HostCmd_CMD_SUPPLICANT_PMK cmd�����cmd body���֣�
 				����������Ҫ������:
 				����������ӵĶ�������оƬ��4�����ֵ���֤
******************************************************************************/
static uint8_t mrvl88w8801_ass_supplicant_pmk_pkg(uint8_t *bssid)
{
    uint16_t pmk_len = 0;
    uint8_t pmk_tlv[64];
    MrvlIEtypes_SSIDParamSet_t *pmk_ssid_tlv;
    MrvlIETypes_BSSIDList_t *pmk_bssid_tlv;
    MrvlIEtypes_PASSPhrase_t *pmk_phrase_tlv;

    pmk_ssid_tlv	=  (MrvlIEtypes_SSIDParamSet_t *)pmk_tlv;
    pmk_ssid_tlv->header.type = TLV_TYPE_SSID;
    pmk_ssid_tlv->header.len = pmrvl88w8801_core->con_info.ssid_len;
    comp_memcpy(pmk_ssid_tlv->ssid,pmrvl88w8801_core->con_info.ssid,pmrvl88w8801_core->con_info.ssid_len);
    pmk_len += sizeof(MrvlIEtypesHeader_t) + pmrvl88w8801_core->con_info.ssid_len;

    pmk_bssid_tlv = (MrvlIETypes_BSSIDList_t *)(pmk_tlv+pmk_len);
    pmk_bssid_tlv->header.type = TLV_TYPE_BSSID;
    pmk_bssid_tlv->header.len = MAC_ADDR_LENGTH;
    comp_memcpy(pmk_bssid_tlv->mac_address,bssid,MAC_ADDR_LENGTH);
    pmk_len += sizeof(MrvlIETypes_BSSIDList_t);

    pmk_phrase_tlv = (MrvlIEtypes_PASSPhrase_t *)(pmk_tlv+pmk_len);
    pmk_phrase_tlv->header.type = TLV_TYPE_PASSPHRASE;
    pmk_phrase_tlv->header.len = pmrvl88w8801_core->con_info.pwd_len;
    comp_memcpy(pmk_phrase_tlv->phrase,pmrvl88w8801_core->con_info.pwd,pmrvl88w8801_core->con_info.pwd_len);
    pmk_len += sizeof(MrvlIEtypesHeader_t) + pmrvl88w8801_core->con_info.pwd_len;

    mrvl88w8801_prepare_cmd(HostCmd_CMD_SUPPLICANT_PMK,HostCmd_ACT_GEN_SET,&pmk_tlv,pmk_len);
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_ret_scan
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		�������������cmd response,ע��������TLV��IEEE��TLV
 				����Marvell��TLV
******************************************************************************/
static uint8_t mrvl88w8801_ret_scan(uint8_t *rx_buffer,int len)
{
    uint8_t j;
    uint8_t index = 0;
    uint16_t ie_size;
    WiFi_Vendor *vendor;
    IEEEType *rates;
    uint8_t *encryption_mode;
    uint8_t ssid[MAX_SSID_LENGTH+1], channel;

    uint8_t vendor_tlv_count = 0;
    IEEEType *vendor_data_ptr[8];
    IEEEType *rsn_data_ptr;

    IEEEType *ie_params;
    WiFi_SecurityType security;
    bss_desc_set_t*bss_desc_set;
    HostCmd_DS_802_11_SCAN_RSP *pscan_rsp = (HostCmd_DS_802_11_SCAN_RSP *)rx_buffer;

    COMP_DEBUG("bss_descript_size %d\n",pscan_rsp->bss_descript_size);
    COMP_DEBUG("number_of_sets %d\n",pscan_rsp->number_of_sets);

    if(pmrvl88w8801_core->con_info.con_status == CON_STA_CONNECTING && pscan_rsp->number_of_sets == 0)
    {
        COMP_DEBUG("WIFI ERR:can not find ap\n");
    }

    /* �ж���������AP���� */
    if (pscan_rsp->number_of_sets > 0)
    {
        bss_desc_set = (bss_desc_set_t *)(pscan_rsp->bss_desc_and_tlv_buffer);
        for (index = 0; index < pscan_rsp->number_of_sets; index++)
        {
            security = WIFI_SECURITYTYPE_WEP;
            rates = NULL;
            ie_params = &bss_desc_set->ie_parameters;
			if(bss_desc_set->ie_length > ((sizeof(bss_desc_set_t) + sizeof(bss_desc_set->ie_length) + sizeof(bss_desc_set->ie_parameters))))
            {
				ie_size = bss_desc_set->ie_length - (sizeof(bss_desc_set_t) - sizeof(bss_desc_set->ie_length) - sizeof(bss_desc_set->ie_parameters));
            }
			else
			{
				ie_size = 0;
			}
			while (ie_size > 0)
            {
                /* �жϸ���TLV */
                switch (ie_params->header.type)
                {
                case TLV_TYPE_SSID:
                    if(ie_params->header.length <= MAX_SSID_LENGTH)
                    {
                        comp_memcpy(ssid, ie_params->data, ie_params->header.length);
                        ssid[ie_params->header.length] = '\0';
                    }
                    else
                    {
                        comp_memcpy(ssid, ie_params->data, MAX_SSID_LENGTH);
                        ssid[MAX_SSID_LENGTH] = '\0';
                    }
                    break;
                case TLV_TYPE_RATES:
                    rates = ie_params;
                    break;
                case TLV_TYPE_PHY_DS:
                    channel = ie_params->data[0];
                    break;
                case TLV_TYPE_RSN_PARAMSET:
                    /* �˲���������Ϊ�յ�RSN����WPA2 */
                    security = WIFI_SECURITYTYPE_WPA2;
                    rsn_data_ptr = ie_params;
                    break;
                case TLV_TYPE_VENDOR_SPECIFIC_IE:
                    if (security != WIFI_SECURITYTYPE_WPA2)
                    {
                        vendor = (WiFi_Vendor *)ie_params->data;
                        if (vendor->oui[0] == 0x00 && vendor->oui[1] == 0x50 && vendor->oui[2] == 0xf2 && vendor->oui_type == 0x01)
                            security = WIFI_SECURITYTYPE_WPA;
                    }
                    if(vendor_tlv_count < 8)
                    {
                        vendor_data_ptr[vendor_tlv_count] = ie_params;
                        vendor_tlv_count++;
                    }
                    break;
                }
                ie_size -= TLV_STRUCTLEN(*ie_params);
                ie_params = (IEEEType *)TLV_NEXT(ie_params);
            }
            if ((bss_desc_set->cap_info & WIFI_CAPABILITY_PRIVACY) == 0)
                security = WIFI_SECURITYTYPE_NONE;

            pmrvl88w8801_core->con_info.security = security;
            COMP_DEBUG("SSID '%s', ", ssid); /* SSID ���� */
            COMP_DEBUG("MAC %02X:%02X:%02X:%02X:%02X:%02X, ", bss_desc_set->bssid[0], bss_desc_set->bssid[1], bss_desc_set->bssid[2], bss_desc_set->bssid[3], bss_desc_set->bssid[4], bss_desc_set->bssid[5]); /* MAC��ַ */
            COMP_DEBUG("RSSI %d, Channel %d\n", bss_desc_set->rssi, channel); /* �ź�ǿ�Ⱥ�ͨ���� */
            COMP_DEBUG("  Capability: 0x%04x (Security: ", bss_desc_set->cap_info);
            switch (security)
            {
            case WIFI_SECURITYTYPE_NONE:
                encryption_mode = "OPEN";
                COMP_DEBUG("Unsecured");
                break;
            case WIFI_SECURITYTYPE_WEP:
                encryption_mode = "WEP";
                COMP_DEBUG("WEP");
                break;
            case WIFI_SECURITYTYPE_WPA:
                encryption_mode = "WPA";
                COMP_DEBUG("WPA");
                break;
            case WIFI_SECURITYTYPE_WPA2:
                encryption_mode = "WPA2";
                COMP_DEBUG("WPA2");
                break;
            }
            COMP_DEBUG(", Mode: ");
            if (bss_desc_set->cap_info & WIFI_CAPABILITY_IBSS)
                COMP_DEBUG("Ad-Hoc");
            else
                COMP_DEBUG("Infrastructure");
            COMP_DEBUG(")\n");

            if (rates != NULL)
            {
                COMP_DEBUG("  Rates:");
                for (j = 0; j < rates->header.length; j++)
                    COMP_DEBUG(" %.1fMbps", (rates->data[j] & 0x7f) * 0.5);
                COMP_DEBUG("\n");
            }


            if(pmrvl88w8801_core->con_info.con_status == CON_STA_CONNECTING )
            {
                /* �����ӵĶ��� */
                uint8_t rate_tlv[] = {0x01,0x00,0x0c,0x00,0x82,0x84,0x8b,0x8c,0x12,0x96,0x98,0x24,0xb0,0x48,0x60,0x6c};
                uint16_t associate_para_len = 0;
                uint8_t associate_para[256];
                MrvlIEtypes_SSIDParamSet_t *ssid_tlv;
                MrvlIETypes_PhyParamDSSet_t *phy_tlv;
                MrvlIETypes_CfParamSet_t *cf_tlv;
                MrvlIETypes_AuthType_t *auth_tlv;
                MrvlIEtypes_ChanListParamSet_t *channel_list;
                ChanScanParamSet_t *channel_list_para;

                ssid_tlv	=  (MrvlIEtypes_SSIDParamSet_t *)associate_para;
                ssid_tlv->header.type = TLV_TYPE_SSID;
                ssid_tlv->header.len = pmrvl88w8801_core->con_info.ssid_len;
                comp_memcpy(ssid_tlv->ssid,ssid,pmrvl88w8801_core->con_info.ssid_len);
                associate_para_len += sizeof(MrvlIEtypesHeader_t) + pmrvl88w8801_core->con_info.ssid_len;

                phy_tlv = (MrvlIETypes_PhyParamDSSet_t *)(associate_para+associate_para_len);
                phy_tlv->header.type = TLV_TYPE_PHY_DS;
                phy_tlv->header.len = 1;
                phy_tlv->channel = channel;
                associate_para_len +=sizeof(MrvlIETypes_PhyParamDSSet_t);


                cf_tlv = (MrvlIETypes_CfParamSet_t *)(associate_para+associate_para_len);
                comp_memset(cf_tlv,0,sizeof(MrvlIETypes_CfParamSet_t));
                cf_tlv->header.type = TLV_TYPE_CF;
                cf_tlv->header.len = sizeof(MrvlIETypes_CfParamSet_t) - sizeof(MrvlIEtypesHeader_t);
                associate_para_len +=sizeof(MrvlIETypes_CfParamSet_t);

                if(security == WIFI_SECURITYTYPE_NONE)
                {
                    auth_tlv = (MrvlIETypes_AuthType_t *)(associate_para+associate_para_len);
                    auth_tlv->header.type = TLV_TYPE_AUTH_TYPE;
                    auth_tlv->header.len = sizeof(MrvlIETypes_AuthType_t) - sizeof(MrvlIEtypesHeader_t);
                    auth_tlv->auth_type = WIFI_AUTH_MODE_OPEN;
                    associate_para_len +=sizeof(MrvlIETypes_AuthType_t);
                }

                channel_list = (MrvlIEtypes_ChanListParamSet_t *)(associate_para+associate_para_len);
                channel_list_para = (ChanScanParamSet_t *)(associate_para+associate_para_len + sizeof(MrvlIEtypesHeader_t));
                channel_list->header.type = TLV_TYPE_CHANLIST;
                channel_list->header.len = sizeof(ChanScanParamSet_t);
                channel_list_para->radio_type = 0;
                channel_list_para->chan_number = channel;
                channel_list_para->chan_scan_mode = 0;
                channel_list_para->min_scan_time = 0;
                channel_list_para->max_scan_time = 200;
                associate_para_len +=sizeof(MrvlIEtypes_ChanListParamSet_t);

                comp_memcpy(associate_para+associate_para_len,rate_tlv,sizeof(rate_tlv));
                associate_para_len += sizeof(rate_tlv);

                if(security >= WIFI_SECURITYTYPE_WPA)
                {
                    MrvlIETypes_Vendor_t *vendor_tlv;
                    MrvlIETypes_RSN_t *rsn_tlv;
                    mrvl88w8801_ass_supplicant_pmk_pkg(bss_desc_set->bssid);

                    for(index = 0; index < vendor_tlv_count; index++)
                    {
                        vendor_tlv = (MrvlIETypes_Vendor_t *)(associate_para+associate_para_len);
                        vendor_tlv->header.type = TLV_TYPE_VENDOR_SPECIFIC_IE;
                        vendor_tlv->header.len = vendor_data_ptr[index]->header.length;
                        comp_memcpy(vendor_tlv->vendor,vendor_data_ptr[index]->data,vendor_tlv->header.len);
                        associate_para_len += sizeof(MrvlIEtypesHeader_t) + vendor_tlv->header.len;
                    }

                    if(security == WIFI_SECURITYTYPE_WPA2)
                    {
                        rsn_tlv = (MrvlIETypes_RSN_t *)(associate_para+associate_para_len);
                        rsn_tlv->header.type = TLV_TYPE_RSN_PARAMSET;
                        rsn_tlv->header.len = rsn_data_ptr->header.length;
                        comp_memcpy(rsn_tlv->rsn,rsn_data_ptr->data,rsn_tlv->header.len);
                        associate_para_len += sizeof(MrvlIEtypesHeader_t) + rsn_tlv->header.len;
                    }
                }

                comp_memcpy(pmrvl88w8801_core->con_info.mac_address,bss_desc_set->bssid,MAC_ADDR_LENGTH);
                pmrvl88w8801_core->con_info.cap_info = bss_desc_set->cap_info;
                //hw_hex_dump((uint8_t *)&associate_para,associate_para_len);
                mrvl88w8801_prepare_cmd(HostCmd_CMD_802_11_ASSOCIATE,HostCmd_ACT_GEN_GET,&associate_para,associate_para_len);

            }
            else
            {
                if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_scan_result)
                    mrvl_wifi_cb->wifi_scan_result(ssid,bss_desc_set->rssi,channel,encryption_mode);
            }
            /* ������һ���ȵ� */
            bss_desc_set = (bss_desc_set_t *)((uint8_t *)bss_desc_set + sizeof(bss_desc_set->ie_length) + bss_desc_set->ie_length);

        }
    }
    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_ret_connect
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		�������������cmd response
******************************************************************************/
static uint8_t mrvl88w8801_ret_connect(uint8_t *rx_buffer,int len)
{
    HostCmd_DS_802_11_ASSOCIATE_RSP *pconnect_rsp = (HostCmd_DS_802_11_ASSOCIATE_RSP *)rx_buffer;
    uint16_t cap = pconnect_rsp->assoc_rsp.Capability;
    switch (cap)
    {
    case 0xfffc:
    //COMP_DEBUG("WIFI ERR:connect timeout\n");
    case 0xfffd:
    //COMP_DEBUG("WIFI ERR:authencition refused\n");
    case 0xfffe:
    //COMP_DEBUG("WIFI ERR:authencition unhandled message\n");
    case 0xffff:
        //COMP_DEBUG("WIFI ERR:internal error\n");

        if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_connect_result)
            mrvl_wifi_cb->wifi_connect_result(1);
        break;


    default:
    {

        COMP_DEBUG("WIFI SUCCESS:connect success cap 0x%x\n",cap);
        if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_connect_result)
            mrvl_wifi_cb->wifi_connect_result(0);
        break;
    }
    }

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_ret_mac_address
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		�˲�����Ҫ�Ǵ���mac cmd reponse������õ�mac����ô������������:
 				1.����pmrvl88w8801_core�ṹ���ж϶�mac_address
 				2.��ʼ��lwip��mac
******************************************************************************/
static uint8_t mrvl88w8801_ret_mac_address(uint8_t *rx_buffer,int len)
{
    HostCmd_DS_802_11_MAC_ADDRESS *pconnect_rsp = (HostCmd_DS_802_11_MAC_ADDRESS *)rx_buffer;
    comp_memcpy(pmrvl88w8801_core->mac_address,pconnect_rsp->mac_addr,MAC_ADDR_LENGTH);
    COMP_DEBUG("mrvl88w8801_ret_mac_address mac dump\n");

    ethernet_sta_driver_init(pmrvl88w8801_core->mac_address);

    if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_init_result)
        mrvl_wifi_cb->wifi_init_result(COMP_ERR_OK);
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_add_mac_address
 * ����:  		mac_address(IN)			-->mac address
 * ����ֵ: 	����ִ�н��
 * ����:		��mac address��ӵ�����ά��������sta�ṹ��������
******************************************************************************/
static uint8_t mrvl88w8801_add_mac_address(uint8_t *mac_address)
{
    uint8_t index = 0;
    for(index = 0; index < WIFI_CONF_MAX_CLIENT_NUM; index++)
    {
        if(pmrvl88w8801_core->sta_mac[index].used == 0)
        {
            comp_memcpy(pmrvl88w8801_core->sta_mac[index].sta_mac_address,mac_address,MAC_ADDR_LENGTH);
            pmrvl88w8801_core->sta_mac[index].used = 1;
            break;
        }
    }
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_del_mac_address
 * ����:  		mac_address(IN)			-->mac address
 * ����ֵ: 	����ִ�н��
 * ����:		��mac address�ӱ���ά��������sta�ṹ��������ɾ��
******************************************************************************/
static uint8_t mrvl88w8801_del_mac_address(uint8_t *mac_address)
{
    uint8_t index = 0;
    for(index = 0; index < WIFI_CONF_MAX_CLIENT_NUM; index++)
    {
        if(comp_memcmp(pmrvl88w8801_core->sta_mac[index].sta_mac_address,mac_address,MAC_ADDR_LENGTH) == 0)
        {
            comp_memset(pmrvl88w8801_core->sta_mac[index].sta_mac_address,0,MAC_ADDR_LENGTH);
            pmrvl88w8801_core->sta_mac[index].used = 0;
            break;
        }
    }
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_process_cmdrsp
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		����������Ӧ
******************************************************************************/
static uint8_t mrvl88w8801_process_cmdrsp(uint8_t *rx_buffer,int len)
{
    HostCmd_DS_COMMAND *resp = (HostCmd_DS_COMMAND *) rx_buffer;
    uint16_t cmdresp_no = resp->command & (~HostCmd_RET_BIT);
    if(resp->result != HostCmd_RESULT_OK)
    {
        return WIFI_ERR_INVALID_RESPONSE;
    }

#if 0
    COMP_DEBUG("RECV C resp\n");
    hw_hex_dump(rx_buffer,len);
#endif
    switch(cmdresp_no)
    {
    case HostCmd_CMD_GET_HW_SPEC:
    {
        mrvl88w8801_ret_get_hw_spec(rx_buffer+CMD_HDR_SIZE,len-CMD_HDR_SIZE);
        mrvl88w8801_prepare_cmd(HostCmd_CMD_802_11_MAC_ADDRESS,HostCmd_ACT_GEN_GET,NULL,0);

        break;
    }
    case HostCmd_CMD_802_11_SCAN:
    {
        mrvl88w8801_ret_scan(rx_buffer+CMD_HDR_SIZE,len-CMD_HDR_SIZE);
        break;
    }
    case HostCmd_CMD_802_11_ASSOCIATE:
    {
        mrvl88w8801_ret_connect(rx_buffer+CMD_HDR_SIZE,len-CMD_HDR_SIZE);
        break;
    }
    case HostCmd_CMD_802_11_DEAUTHENTICATE:
    {
        /* STA��������ֻ���յ�cmd response */
        break;
    }
    case HostCmd_CMD_802_11_MAC_ADDRESS:
    {
        mrvl88w8801_ret_mac_address(rx_buffer+CMD_HDR_SIZE,len-CMD_HDR_SIZE);
        break;
    }
    case HostCmd_CMD_SUPPLICANT_PMK:
    {
        uint16_t tx_len = mrvl_tx_buffer[0] | (mrvl_tx_buffer[1] << 8);
        hw_sdio_cmd53(SDIO_EXCU_WRITE,SDIO_FUNC_1,pmrvl88w8801_core->control_io_port +CTRL_PORT,0,mrvl_tx_buffer,tx_len);

        break;
    }
    case HostCmd_CMD_MAC_CONTROL:
    {
        mrvl88w8801_prepare_cmd(HostCmd_CMD_GET_HW_SPEC,HostCmd_ACT_GEN_GET,NULL,0);

        break;
    }
    case HostCmd_CMD_FUNC_INIT:
    {
        uint16_t mac_control_action = HostCmd_ACT_MAC_RX_ON | HostCmd_ACT_MAC_TX_ON |HostCmd_ACT_MAC_ETHERNETII_ENABLE;
        mrvl88w8801_prepare_cmd(HostCmd_CMD_MAC_CONTROL,HostCmd_ACT_GEN_SET,(void *)&mac_control_action,2);
        break;
    }
    case HOST_CMD_APCMD_SYS_CONFIGURE:
    {
        mrvl88w8801_prepare_cmd(HOST_CMD_APCMD_BSS_START,HostCmd_ACT_GEN_GET,NULL,0);
        break;
    }
    case HOST_CMD_APCMD_BSS_START:
    {
        pmrvl88w8801_core->bss_type = BSS_TYPE_UAP;
        if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_ap_connect_result)
            ethernet_dhcpd_start(mrvl_wifi_cb->wifi_ap_connect_result);
        else
            ethernet_dhcpd_start(NULL);
        break;
    }
    case HOST_CMD_APCMD_BSS_STOP:
    {
        pmrvl88w8801_core->bss_type = BSS_TYPE_STA;
        ethernet_dhcpd_stop();
        break;
    }
    default:
    {
        COMP_DEBUG("WIFI:invalid cmd id resp\n");
        break;
    }
    }
    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_process_event
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		�����յ���event
******************************************************************************/
static uint8_t mrvl88w8801_process_event(uint8_t *rx_buffer,int len)
{
    uint16_t event = *(uint16_t *)(rx_buffer + CMD_SDIO_HDR_SIZE);

    COMP_DEBUG("WIFI EVENT ID 0x%x\n",event);
    switch(event)
    {
    case EVENT_PORT_RELEASE:
    {
        COMP_DEBUG("EVENT_PORT_RELEASE\n");
        COMP_DEBUG("WIFI:connected mac:\n");
        hw_hex_dump(pmrvl88w8801_core->remote_mac_address,MAC_ADDR_LENGTH);
        ethernet_link_up();
        ethernet_dhcp_start();
        break;
    }
    case EVENT_DEAUTHENTICATED:
    {
        /* AP��STA�Ƴ�����AP�رգ�STA�����յ�����Ϣ */
        COMP_DEBUG("EVENT_DEAUTHENTICATED\n");
        comp_memset(pmrvl88w8801_core->remote_mac_address,0,MAC_ADDR_LENGTH);
        ethernet_link_down();
        break;
    }
    case EVENT_WMM_STATUS_CHANGE:
    {
        COMP_DEBUG("EVENT_WMM_STATUS_CHANGE\n");
        break;
    }
    case EVENT_MICRO_AP_BSS_ACTIVE:
    {
        COMP_DEBUG("EVENT_MICRO_AP_BSS_ACTIVE\n");
        break;
    }
    case EVENT_MICRO_AP_STA_DEAUTH:
    {
        /* ������AP����STA�����Ͽ����߰��Ӱ�STA�ߵ����᷵�ص�event */
        COMP_DEBUG("EVENT_MICRO_AP_STA_DEAUTH\n");
        mrvl88w8801_del_mac_address(&rx_buffer[10]);
        if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_ap_disconnect_result)
            mrvl_wifi_cb->wifi_ap_disconnect_result(&rx_buffer[10]);
        break;
    }
    case EVENT_MICRO_AP_STA_ASSOC:
    {
        /* ������AP����STA�����ӷ��ص�event */
        COMP_DEBUG("EVENT_MICRO_AP_STA_ASSOC\n");
        mrvl88w8801_add_mac_address(&rx_buffer[10]);
        break;
    }
    case MICRO_AP_EV_RSN_CONNECT:
    {
        /* ������WPA/WPA2 AP����STA���ӳɹ����ص�event */
        COMP_DEBUG("MICRO_AP_EV_RSN_CONNECT\n");
        break;
    }
    case EVENT_MICRO_AP_BSS_START:
    {
        COMP_DEBUG("EVENT_MICRO_AP_BSS_START\n");
        if(mrvl_wifi_cb && mrvl_wifi_cb->wifi_start_ap_result)
            mrvl_wifi_cb->wifi_start_ap_result(0);
        break;
    }
    default:
    {
        COMP_DEBUG("UNKNOWD EVENT ID\n");
        break;
    }
    }
    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_process_data
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		�����յ���data(Ҳ����tcp/ip data)
******************************************************************************/
static uint8_t mrvl88w8801_process_data(uint8_t *rx_buffer,int len)
{

    if(pmrvl88w8801_core->bss_type == BSS_TYPE_UAP)
    {
        RxPD *rx_packet = (RxPD *)rx_buffer;
        uint16_t rx_payload_len = rx_packet->rx_pkt_length;
        uint8_t *payload = (uint8_t *)((uint8_t *)rx_packet + rx_packet->rx_pkt_offset + CMD_SDIO_HDR_SIZE);
        RxPacketHdr_t *rx_hdr = (RxPacketHdr_t *)payload;

#if 0
        COMP_DEBUG("Rx dest %02x:%02x:%02x:%02x:%02x:%02x\n",
                   rx_hdr->eth803_hdr.dest_addr[0], rx_hdr->eth803_hdr.dest_addr[1],
                   rx_hdr->eth803_hdr.dest_addr[2], rx_hdr->eth803_hdr.dest_addr[3],
                   rx_hdr->eth803_hdr.dest_addr[4], rx_hdr->eth803_hdr.dest_addr[5]);

        COMP_DEBUG("local mac %02x:%02x:%02x:%02x:%02x:%02x\n",
                   pmrvl88w8801_core->mac_address[0], pmrvl88w8801_core->mac_address[1],
                   pmrvl88w8801_core->mac_address[2], pmrvl88w8801_core->mac_address[3],
                   pmrvl88w8801_core->mac_address[4], pmrvl88w8801_core->mac_address[5]);
#endif
        if (rx_hdr->eth803_hdr.dest_addr[0] & 0x01)
        {
            /* 1.�㲥�����ת�� */
            uint8_t *buffer = mrvl88w8801_get_send_data_buf();
            uint8_t *payload = (uint8_t *)((uint8_t *)rx_packet + rx_packet->rx_pkt_offset + 4);
            comp_memcpy(buffer,payload,rx_payload_len);
            mrvl88w8801_send_date(buffer,rx_payload_len);

            /* 2.�Լ�AP����㲥��� */
            ethernet_lwip_process(rx_buffer,len);
        }
        else
        {
            /* ������� */
            if(hw_memcmp(pmrvl88w8801_core->mac_address,rx_hdr->eth803_hdr.dest_addr,MAC_ADDR_LENGTH) == 0)
            {
                /* ����Ǳ��ص�ַ������LWIP���� */
                //COMP_DEBUG("AP mode:dest mac is AP board\n");
                ethernet_lwip_process(rx_buffer,len);

            }
            else
            {
                /* �ж��Ƿ���STA��ַ ��ת�� */
                uint8_t *buffer = mrvl88w8801_get_send_data_buf();
                uint8_t *payload = (uint8_t *)((uint8_t *)rx_packet + rx_packet->rx_pkt_offset + 4);
                comp_memcpy(buffer,payload,rx_payload_len);
                mrvl88w8801_send_date(buffer,rx_payload_len);
            }
        }

    }

    if(pmrvl88w8801_core->bss_type == BSS_TYPE_STA)
    {
        ethernet_lwip_process(rx_buffer,len);
    }

    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_parse_rx_packet
 * ����:  		rx_buffer(IN)			-->rx buffer
 				len(IN)				-->rx buffer len
 * ����ֵ: 	����ִ�н��
 * ����:		������оƬ�յ������ݣ��ٷֱ���cmd response/event/data
 				��������������
******************************************************************************/
static uint8_t mrvl88w8801_parse_rx_packet(uint8_t *rx_buffer,int len)
{
    uint16_t rx_type = (uint16_t)rx_buffer[2];
    switch(rx_type)
    {
    case TYPE_DATA:
    {
        mrvl88w8801_process_data(rx_buffer,len);
        break;
    }
    case TYPE_CMD_CMDRSP:
    {
        mrvl88w8801_process_cmdrsp(rx_buffer,len);

        break;
    }

    case TYPE_EVENT:
    {
        mrvl88w8801_process_event(rx_buffer,len);
        break;
    }
    default:
    {
        COMP_DEBUG("ERR:unkown rx type\n");
        break;
    }
    }


    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_get_read_port
 * ����:  		port(OUT)			-->����read port
 * ����ֵ: 	����ִ�н��
 * ����:		���ڻ�ȡread port
******************************************************************************/
static uint8_t mrvl88w8801_get_read_port(uint8_t *port)
{
    if (pmrvl88w8801_core->read_bitmap & CTRL_PORT_MASK)
    {
        pmrvl88w8801_core->read_bitmap &= (uint16_t) (~CTRL_PORT_MASK);
        *port = CTRL_PORT;
    }
    else
    {

        if (pmrvl88w8801_core->read_bitmap & (1 << pmrvl88w8801_core->curr_rd_port))
        {
            pmrvl88w8801_core->read_bitmap &=(uint16_t) (~(1 << pmrvl88w8801_core->curr_rd_port));
            *port = pmrvl88w8801_core->curr_rd_port;

            /* hw rx wraps round only after port (MAX_PORT-1) */
            if (++pmrvl88w8801_core->curr_rd_port == MAX_PORT)
                /* port 0 is reserved for cmd port */
                pmrvl88w8801_core->curr_rd_port = 1;
        }
        else
        {
            //COMP_DEBUG("mrvl88w8801_get_read_port error\n");
            return WIFI_ERR_NO_MORE_READ_HANDLE;
        }
    }
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_get_write_port
 * ����:  		port(OUT)			-->����read port
 * ����ֵ: 	����ִ�н��
 * ����:		���ڻ�ȡwrite port
******************************************************************************/
static uint8_t mrvl88w8801_get_write_port(uint8_t *port)
{
    if (pmrvl88w8801_core->write_bitmap & (1 << pmrvl88w8801_core->curr_wr_port))
    {
        pmrvl88w8801_core->write_bitmap &= (uint32_t) (~(1 << pmrvl88w8801_core->curr_wr_port));
        *port = pmrvl88w8801_core->curr_wr_port;
        if (++pmrvl88w8801_core->curr_wr_port == pmrvl88w8801_core->mp_end_port)
            pmrvl88w8801_core->curr_wr_port = 1;
    }
    else
    {
    }
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_handle_interrupt
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		����download/upload interrupt���Ĵ���
******************************************************************************/
static uint8_t mrvl88w8801_handle_interrupt()
{
    uint8_t int_status;
    /* ��ctrl�Ĵ��� */
    hw_sdio_cmd53(SDIO_EXCU_READ,SDIO_FUNC_1,REG_PORT,0,pmrvl88w8801_core->mp_regs,64);
    //hw_hex_dump(pmrvl88w8801_core->mp_regs,256);

    /* ��ȡ�жϼĴ�����״̬ */
    int_status = pmrvl88w8801_core->mp_regs[HOST_INT_STATUS_REG];

    /* ���ж� */
    hw_sdio_cmd52(SDIO_EXCU_WRITE,SDIO_FUNC_1,HOST_INT_STATUS_REG,INT_STATUS_ALL & ~int_status,NULL);

    pmrvl88w8801_core->read_bitmap = (uint16_t) pmrvl88w8801_core->mp_regs[RD_BITMAP_L];
    pmrvl88w8801_core->read_bitmap |=((uint16_t) pmrvl88w8801_core->mp_regs[RD_BITMAP_U]) << 8;
    //COMP_DEBUG("read bitmap 0x%x\n",pmrvl88w8801_core->read_bitmap);

    pmrvl88w8801_core->write_bitmap = (uint16_t) pmrvl88w8801_core->mp_regs[WR_BITMAP_L];
    pmrvl88w8801_core->write_bitmap |=((uint16_t) pmrvl88w8801_core->mp_regs[WR_BITMAP_U]) << 8;
    //COMP_DEBUG("write bitmap 0x%x\n",pmrvl88w8801_core->write_bitmap);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_get_hw_spec
 * ����:  		tx(IN)				-->tx buffer
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_GET_HW_SPEC command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_get_hw_spec(uint8_t* tx)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_GET_HW_SPEC);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_GET_HW_SPEC;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_mac_control
 * ����:  		tx(IN)				-->tx buffer
 				data_buff(IN)		-->action��Ϊָ��
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_MAC_CONTROL command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_mac_control(uint8_t* tx,void *data_buff)
{
    uint16_t action = *((uint16_t *) data_buff);
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_MAC_CONTROL *pmac = &cmd->params.mac_ctrl;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_MAC_CONTROL);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_MAC_CONTROL;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;
    pmac->action = comp_cpu_to_le16(action);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_func_init
 * ����:  		tx(IN)				-->tx buffer
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_FUNC_INIT command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_func_init(uint8_t* tx)
{
    uint16_t tx_packet_len = CMD_HDR_SIZE;
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_FUNC_INIT;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_scan_prepare
 * ����:  		tx(IN)				-->tx buffer
 				data_buff(IN)		-->������cmd body�����������channel list
 				data_len(IN)		-->������cmd body len,���������channel list����
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_802_11_SCAN command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_scan_prepare(uint8_t* tx,void *data_buff,uint16_t data_len)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_802_11_SCAN *pscan = &cmd->params.scan;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_802_11_SCAN)+data_len-sizeof(pscan->tlv_buffer);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_802_11_SCAN;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;
    pscan->bss_mode = HostCmd_BSS_MODE_ANY;
    comp_memset(pscan->bssid,0,MAC_ADDR_LENGTH);
    comp_memcpy(pscan->tlv_buffer,data_buff,data_len);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_connect_prepare
 * ����:  		tx(IN)				-->tx buffer
 				data_buff(IN)		-->������cmd body�����������channel list
 				data_len(IN)		-->������cmd body len,���������channel list����
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_802_11_ASSOCIATE command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_connect_prepare(uint8_t* tx,void *data_buff,uint16_t data_len)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_802_11_ASSOCIATE *passociate = &cmd->params.associate;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_802_11_ASSOCIATE)+data_len-sizeof(passociate->tlv_buffer);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_802_11_ASSOCIATE;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;
    comp_memcpy(passociate->peer_sta_addr,pmrvl88w8801_core->con_info.mac_address,MAC_ADDR_LENGTH);
    comp_memcpy(pmrvl88w8801_core->remote_mac_address,pmrvl88w8801_core->con_info.mac_address,MAC_ADDR_LENGTH);
    passociate->cap_info = pmrvl88w8801_core->con_info.cap_info;
    passociate->listen_interval = 0xa;
    passociate->beacon_period = 0x40;
    passociate->dtim_period = 0x0;

    comp_memcpy(passociate->tlv_buffer,data_buff,data_len);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_connect_prepare
 * ����:  		tx(IN)				-->tx buffer
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_802_11_DEAUTHENTICATE command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_disconnect_prepare(uint8_t* tx)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_802_11_DEAUTHENTICATE *pdeauth = &cmd->params.deauth;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_802_11_DEAUTHENTICATE);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_802_11_DEAUTHENTICATE;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;
    comp_memcpy(pdeauth->mac_addr,pmrvl88w8801_core->remote_mac_address,MAC_ADDR_LENGTH);
    pdeauth->reason_code = 36;

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_disconnect_sta_prepare
 * ����:  		tx(IN)				-->tx buffer
 				data_buff(IN)		-->������cmd body�����������channel list
 				data_len(IN)		-->������cmd body len,���������channel list����
 * ����ֵ: 	����ִ�н��
 * ����:		��HOST_CMD_APCMD_STA_DEAUTH command�ķ������Ҫ����AP�ߵ�STA
******************************************************************************/
static uint8_t mrvl88w8801_disconnect_sta_prepare(uint8_t* tx,void *data_buff,uint16_t data_len)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_802_11_DEAUTHENTICATE *pdeauth = &cmd->params.deauth;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_802_11_DEAUTHENTICATE);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HOST_CMD_APCMD_STA_DEAUTH;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = BSS_TYPE_UAP << 4;
    cmd->result = 0;
    comp_memcpy(pdeauth->mac_addr,data_buff,data_len);
    pdeauth->reason_code = 2;

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_mac_addr_prepare
 * ����:  		tx(IN)				-->tx buffer
 				cmd_action(IN)	-->set/get
 				data_buff(IN)		-->mac address���˲�����Ҫ����set
 				data_len(IN)		-->mac�ĳ���
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_802_11_MAC_ADDRESS command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_mac_addr_prepare(uint8_t* tx,uint16_t cmd_action,void *data_buff,uint16_t data_len)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_802_11_MAC_ADDRESS *pmac_addr = &cmd->params.mac_addr;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_802_11_MAC_ADDRESS)+data_len;

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_802_11_MAC_ADDRESS;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;
    if(cmd_action == HostCmd_ACT_GEN_GET)
    {
        pmac_addr->action = HostCmd_ACT_GEN_GET;
        comp_memset(pmac_addr->mac_addr,0,MAC_ADDR_LENGTH);
    }
    else	/* set mac address */
    {
        pmac_addr->action = HostCmd_ACT_GEN_SET;
        comp_memcpy(pmac_addr->mac_addr,data_buff,MAC_ADDR_LENGTH);
    }
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_supplicant_pmk_prepare
 * ����:  		tx(IN)				-->tx buffer
 				cmd_action(IN)	-->set/get
 				data_buff(IN)		-->mac address���˲�����Ҫ����set
 				data_len(IN)		-->mac�ĳ���
 * ����ֵ: 	����ִ�н��
 * ����:		��HostCmd_CMD_SUPPLICANT_PMK command�ķ��
******************************************************************************/
static uint8_t mrvl88w8801_supplicant_pmk_prepare(uint8_t* tx,uint16_t cmd_action,void *data_buff,uint16_t data_len)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_802_11_SUPPLICANT_PMK *psupplicant_pmk = &cmd->params.esupplicant_psk;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(HostCmd_DS_802_11_SUPPLICANT_PMK)+data_len-sizeof(psupplicant_pmk->tlv_buffer);

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HostCmd_CMD_SUPPLICANT_PMK;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = 0;
    cmd->result = 0;
    if(cmd_action == HostCmd_ACT_GEN_GET)
    {
        psupplicant_pmk->action = HostCmd_ACT_GEN_GET;
    }
    else	/* set mac address */
    {
        psupplicant_pmk->action = HostCmd_ACT_GEN_SET;

    }
    psupplicant_pmk->cache_result = 0;
    comp_memcpy(psupplicant_pmk->tlv_buffer,data_buff,data_len);
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_cre_ap_prepare
 * ����:  		tx(IN)				-->tx buffer
 * ����ֵ: 	����ִ�н��
 * ����:		��HOST_CMD_APCMD_BSS_START command�ķ������Ҫ���ڿ����ȵ�
******************************************************************************/
static uint8_t mrvl88w8801_cre_ap_prepare(uint8_t* tx)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    uint16_t tx_packet_len = CMD_HDR_SIZE;

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HOST_CMD_APCMD_BSS_START;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = BSS_TYPE_UAP << 4;
    cmd->result = 0;
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_stop_ap_prepare
 * ����:  		tx(IN)				-->tx buffer
 * ����ֵ: 	����ִ�н��
 * ����:		��HOST_CMD_APCMD_BSS_STOP command�ķ������Ҫ����ֹͣ�ȵ�
******************************************************************************/
static uint8_t mrvl88w8801_stop_ap_prepare(uint8_t* tx)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    uint16_t tx_packet_len = CMD_HDR_SIZE;

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HOST_CMD_APCMD_BSS_STOP;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = BSS_TYPE_UAP << 4;
    cmd->result = 0;
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_sys_conf_prepare
 * ����:  		tx(IN)				-->tx buffer
 				cmd_action(IN)	-->set/get
 				data_buff(IN)		-->mac address���˲�����Ҫ����set
 				data_len(IN)		-->mac�ĳ���
 * ����ֵ: 	����ִ�н��
 * ����:		��HOST_CMD_APCMD_SYS_CONFIGURE command�ķ��
 				��Ҫ�����ڴ���AP�ȵ�֮ǰ����ǰ���ú�AP�ȵ��һЩ��Ϣ
******************************************************************************/
static uint8_t mrvl88w8801_sys_conf_prepare(uint8_t* tx,uint16_t cmd_action,void *data_buff,uint16_t data_len)
{
    HostCmd_DS_COMMAND *cmd = (HostCmd_DS_COMMAND *)tx;
    HostCmd_DS_SYS_CONFIG *sys_conf= &cmd->params.sys_config;
    uint16_t tx_packet_len = CMD_HDR_SIZE + sizeof(uint16_t) + data_len;

    cmd->pack_len = tx_packet_len;
    cmd->pack_type = TYPE_CMD_CMDRSP;
    cmd->command = HOST_CMD_APCMD_SYS_CONFIGURE;
    cmd->size = tx_packet_len - CMD_SDIO_HDR_SIZE;
    cmd->seq_num = 0;
    cmd->bss = BSS_TYPE_UAP << 4;;
    cmd->result = 0;
    if(cmd_action == HostCmd_ACT_GEN_GET)
    {
        sys_conf->action = HostCmd_ACT_GEN_GET;
    }
    else	/* set mac address */
    {
        sys_conf->action = HostCmd_ACT_GEN_SET;

    }
    comp_memcpy(sys_conf->tlv_buffer,data_buff,data_len);
    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_prepare_cmd
 * ����:  		cmd_id(IN)				-->cmd id
 				cmd_action(IN)		-->cmd action(SET/GET)
 				data_buff(IN)			-->��Ҫ�������body����
 				data_len(IN)			-->����body���ֵĳ���
 * ����ֵ: 	����ִ�н��
 * ����:		����
******************************************************************************/
static uint8_t mrvl88w8801_prepare_cmd(uint16_t cmd_id,uint16_t cmd_action,void *data_buff,uint16_t data_len)
{
    uint8_t *tx = mrvl_tx_buffer;
    uint16_t tx_len;
    /* ������� */
    switch(cmd_id)
    {
    case HostCmd_CMD_GET_HW_SPEC:
    {
        mrvl88w8801_get_hw_spec(tx);
        break;
    }
    case HostCmd_CMD_MAC_CONTROL:
    {
        mrvl88w8801_mac_control(tx,data_buff);
        break;
    }
    case HostCmd_CMD_FUNC_INIT:
    {
        /* FUNC INIT������ */
        mrvl88w8801_func_init(tx);
        break;
    }
    case HostCmd_CMD_802_11_SCAN:
    {
        mrvl88w8801_scan_prepare(tx,data_buff,data_len);
        break;
    }
    case HostCmd_CMD_802_11_ASSOCIATE:
    {
        mrvl88w8801_connect_prepare(tx,data_buff,data_len);
        break;
    }
    case HostCmd_CMD_802_11_DEAUTHENTICATE:
    {
        mrvl88w8801_disconnect_prepare(tx);
        break;
    }
    case HOST_CMD_APCMD_STA_DEAUTH:
    {
        mrvl88w8801_disconnect_sta_prepare(tx,data_buff,data_len);
        break;
    }
    case HostCmd_CMD_802_11_MAC_ADDRESS:
    {
        mrvl88w8801_mac_addr_prepare(tx,cmd_action,data_buff,data_len);
        break;
    }
    case HostCmd_CMD_SUPPLICANT_PMK:
    {
        mrvl88w8801_supplicant_pmk_prepare(tx,cmd_action,data_buff,data_len);
        break;
    }
    case HOST_CMD_APCMD_BSS_START:
    {
        mrvl88w8801_cre_ap_prepare(tx);
        break;
    }
    case HOST_CMD_APCMD_BSS_STOP:
    {
        mrvl88w8801_stop_ap_prepare(tx);
        break;
    }
    case HOST_CMD_APCMD_SYS_CONFIGURE:
    {
        mrvl88w8801_sys_conf_prepare(tx,cmd_action,data_buff,data_len);
        break;
    }
    default:
    {
        COMP_DEBUG("WIFI:invalid cmd id\n");
        break;
    }

    }

    if(pmrvl88w8801_core->con_info.security >= WIFI_SECURITYTYPE_WPA && cmd_id == HostCmd_CMD_802_11_ASSOCIATE)
    {
    }
    else
    {
        tx_len = tx[0] | (tx[1] << 8);
#if 0
        COMP_DEBUG("SEND C\n");
        hw_hex_dump(tx,tx_len);
#endif
        hw_sdio_cmd53(SDIO_EXCU_WRITE,SDIO_FUNC_1,pmrvl88w8801_core->control_io_port +CTRL_PORT,0,tx,tx_len);
    }

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_core_init
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		pmrvl88w8801_core�ṹ���ʼ��
******************************************************************************/
static uint8_t mrvl88w8801_core_init()
{
    comp_memset(pmrvl88w8801_core,0,sizeof(mrvl88w8801_core_t));
    pmrvl88w8801_core->bss_type = BSS_TYPE_STA;
    pmrvl88w8801_core->mp_regs = mp_reg_array;
    pmrvl88w8801_core->curr_wr_port = 1;
    pmrvl88w8801_core->curr_rd_port = 1;
    return COMP_ERR_OK;
}


/******************************************************************************
 *	������:	mrvl88w8801_get_control_io_port
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		ͨ��IO_PORT_0_REG IO_PORT_1_REG IO_PORT_2_REG�õ�
 				control io port(Ҳ����cmd/cmd resp/fw data�Ķ�д�Ĵ�����ַ)
******************************************************************************/
static uint8_t mrvl88w8801_get_control_io_port()
{
    uint32_t control_io_port = 0;
    uint8_t temp_op_port = 0;;

    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,IO_PORT_0_REG,0,&temp_op_port);
    control_io_port |= temp_op_port;
    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,IO_PORT_1_REG,0,&temp_op_port);
    control_io_port |= (temp_op_port<<8);
    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,IO_PORT_2_REG,0,&temp_op_port);
    control_io_port |= (temp_op_port<<16);

    COMP_DEBUG("WIFI:io_port 0x%x\n",control_io_port);
    pmrvl88w8801_core->control_io_port = control_io_port;
    return COMP_ERR_OK;

}

/******************************************************************************
 *	������:	mrvl88w8801_get_fw_status
 * ����:  		fw_status(OUT)			-->��ȡ��fw status
 * ����ֵ: 	����ִ�н��
 * ����:		��ȡfw status
******************************************************************************/
static uint8_t mrvl88w8801_get_fw_status(uint16_t *fw_status)
{
    uint8_t fws0 = 0, fws1 = 0;

    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,CARD_FW_STATUS0_REG,0,&fws0);
    hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,CARD_FW_STATUS1_REG,0,&fws1);
    *fw_status = (uint16_t)((fws1 << 8) | fws0);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_download_fw
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		����firmware��оƬ
******************************************************************************/
static uint8_t mrvl88w8801_download_fw()
{
    uint16_t index;
    uint16_t fw_status;
    const uint8_t *fw_data = mrvl88w8801_fw;
    uint32_t fw_len = sizeof(mrvl88w8801_fw);
    uint16_t fw_req_size = 0;
    uint8_t fw_req_temp_size = 0;
    uint32_t control_io_port = pmrvl88w8801_core->control_io_port;

#if 0
    /* Check firmware active */
    for(index = 0; index < WIFI_CONF_MIN_POLL_TRIES; index++)
    {
        mrvl88w8801_get_fw_status(&fw_status);
        if (fw_status == FIRMWARE_READY)
        {
            COMP_DEBUG("WIFI:fw is active index %d\n",index);
            return COMP_ERR_OK;
        }
    }
#endif

    while (fw_len)
    {
        fw_req_size = 0;
        fw_req_temp_size = 0;
        if(fw_len != sizeof(mrvl88w8801_fw))
        {
            /* polling card status */
            mrvl88w8801_download_fw_wait();
        }

        /* Get next block length */
        hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,READ_BASE_0_REG,0,&fw_req_temp_size);
        fw_req_size |= fw_req_temp_size;
        hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,READ_BASE_1_REG,0,&fw_req_temp_size);
        fw_req_size |= (fw_req_temp_size<<8);

        if(fw_req_size == 0)
            continue;

        //COMP_DEBUG("WIFI:Required: %d bytes, Remaining: %d bytes\r\n", fw_req_size, fw_len);
        if (fw_req_size & 1)
        {
            /* ��sizeΪ����(��17)��˵�����շ�������CRCУ����� */
            COMP_DEBUG("WIFI:Error: an odd size is invalid!\n");
            return WIFI_ERR_REQ_INVALID_FW_SIZE;
        }

        if (fw_req_size > fw_len)
            fw_req_size = fw_len;

        printf("fw_req_size:%d\n\r", fw_req_size);
        /* Write block */
        hw_sdio_cmd53(SDIO_EXCU_WRITE,SDIO_FUNC_1,control_io_port,0,(uint8_t *)fw_data,fw_req_size);

        fw_len -= fw_req_size;
        fw_data += fw_req_size;
    }

    /* Check firmware active */
    for(index = 0; index < WIFI_CONF_MAX_POLL_TRIES; index++)
    {
        mrvl88w8801_get_fw_status(&fw_status);
        if (fw_status == FIRMWARE_READY)
        {
            COMP_DEBUG("WIFI:fw is active index %d\n",index);
            return COMP_ERR_OK;
        }
    }

    return WIFI_ERR_INVALID_FW_STATUS;

}

/******************************************************************************
 *	������:	mrvl88w8801_enable_host_int
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		ͨ��CMD52����download,upload�ж�
******************************************************************************/
static uint8_t mrvl88w8801_enable_host_int()
{
    hw_sdio_cmd52(SDIO_EXCU_WRITE,SDIO_FUNC_1,HOST_INT_MASK_REG,HIM_ENABLE,NULL);

    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_init_fw
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		ִ��init fw cmd�������������ȷ���һ������յ�cmd response��
 				ʱ������һ��CMD
******************************************************************************/
static uint8_t mrvl88w8801_init_fw()
{
    mrvl88w8801_prepare_cmd(HostCmd_CMD_FUNC_INIT,HostCmd_ACT_GEN_SET,NULL,0);
    return COMP_ERR_OK;
}

/******************************************************************************
 *	������:	mrvl88w8801_download_fw_wait
 * ����:  		NULL
 * ����ֵ: 	����ִ�н��
 * ����:		polling card status����ȡ�Ĵ���CARD_TO_HOST_EVENT_REG 0x30��״̬
 				�ж�bit0,bit3�Ƿ񶼱���λ
******************************************************************************/
static uint8_t mrvl88w8801_download_fw_wait()
{
    uint8_t status = 0;;
    while(1)
    {
        hw_sdio_cmd52(SDIO_EXCU_READ,SDIO_FUNC_1,CARD_TO_HOST_EVENT_REG,0,&status);
        if((status & CARD_IO_READY) == CARD_IO_READY && (status & DN_LD_CARD_RDY) == DN_LD_CARD_RDY)
            break;
    }

    return COMP_ERR_OK;
}
